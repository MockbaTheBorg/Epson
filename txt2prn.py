#!/usr/bin/env python3
"""
ASCII Art to Epson LX-80 Printer File Converter
Converts text files with asterisks and spaces to Epson ESC/P graphics format
"""

import sys
import argparse


def read_ascii_art(filename):
    """
    Read ASCII art file and convert to 2D array
    Asterisks (*) become black pixels, spaces become white pixels
    
    Returns:
        2D list of booleans (True = black pixel)
    """
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
        
        # Remove trailing newlines but preserve structure
        lines = [line.rstrip('\n\r') for line in lines]
        
        if not lines:
            raise ValueError("Empty file")
        
        # Find maximum line length
        max_width = max(len(line) for line in lines)
        
        # Convert to 2D boolean array
        pixels = []
        for line in lines:
            # Pad line to max_width
            padded_line = line.ljust(max_width)
            # Convert to booleans (True for asterisk, False for space)
            row = [char not in (' ', '\t') for char in padded_line]
            pixels.append(row)
        
        return pixels
    
    except FileNotFoundError:
        print(f"Error: Could not find input file '{filename}'")
        sys.exit(1)
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)


def ascii_to_epson_graphics(pixels, dpi_mode='60', scale=1):
    """
    Convert 2D pixel array to Epson graphics bytes
    
    Args:
        pixels: 2D list of booleans (True = black)
        dpi_mode: '60' for ESC K (60 dpi), '120' for ESC L (120 dpi)
        scale: Integer scaling factor (1=normal, 2=double size, etc.)
    
    Returns:
        List of bytes for printer
    """
    ESC = 0x1B
    CR = 0x0D
    LF = 0x0A
    
    # Graphics mode command
    if dpi_mode == '120':
        gfx_cmd = 0x4C  # ESC L - 120 dpi
    else:
        gfx_cmd = 0x4B  # ESC K - 60 dpi
    
    height = len(pixels)
    width = len(pixels[0]) if height > 0 else 0
    
    if width == 0 or height == 0:
        raise ValueError("Empty image data")
    
    # Apply scaling
    if scale > 1:
        scaled_pixels = []
        for row in pixels:
            # Repeat each row 'scale' times
            for _ in range(scale):
                scaled_row = []
                for pixel in row:
                    # Repeat each pixel 'scale' times
                    scaled_row.extend([pixel] * scale)
                scaled_pixels.append(scaled_row)
        pixels = scaled_pixels
        height = len(pixels)
        width = len(pixels[0])
    
    data = []
    
    # Set printer LPI to 8/72 inch
    data.append(ESC)
    data.append(0x41)  # ESC A
    data.append(0x08)  # 8/72 inch LPI
        
    # Process image in strips of 8 pixels high (one byte per column)
    for y in range(0, height, 8):
        # Start graphics mode
        data.append(ESC)
        data.append(gfx_cmd)
        
        # Send width as low byte, high byte
        data.append(width & 0xFF)
        data.append((width >> 8) & 0xFF)
        
        # Process each column
        for x in range(width):
            byte_val = 0
            # Pack 8 vertical pixels into one byte
            for bit in range(8):
                if y + bit < height:
                    # Check if pixel is black (True)
                    if pixels[y + bit][x]:
                        byte_val |= (1 << (7 - bit))
            
            data.append(byte_val)
        
        # Line feed
        data.append(CR)
        data.append(LF)
    
    # Reset printer
    data.append(ESC)
    data.append(0x40)  # ESC @
        
    return data


def ascii_to_prn(input_file, output_prn, dpi='60', scale=1, add_border=False):
    """
    Convert ASCII art file to Epson LX-80 PRN file
    
    Args:
        input_file: Path to input text file
        output_prn: Path to output PRN file
        dpi: '60' or '120' for graphics resolution
        scale: Integer scaling factor
        add_border: Add a border around the image
    """
    try:
        # Read ASCII art
        pixels = read_ascii_art(input_file)
        
        height = len(pixels)
        width = len(pixels[0]) if height > 0 else 0
        
        print(f"Read ASCII art: {width}x{height} characters")
        
        # Add border if requested
        if add_border:
            border_pixels = []
            # Top border
            border_pixels.append([True] * (width + 2))
            # Content with side borders
            for row in pixels:
                border_pixels.append([True] + row + [True])
            # Bottom border
            border_pixels.append([True] * (width + 2))
            pixels = border_pixels
            width += 2
            height += 2
            print(f"Added border: {width}x{height} characters")
        
        if scale > 1:
            print(f"Scaling by {scale}x: {width*scale}x{height*scale} pixels")
        
        # Check if width is reasonable for printer
        final_width = width * scale
        if final_width > 960:
            print(f"Warning: Image width ({final_width}) may be too wide for printer")
            print("Consider using a smaller scale factor")
        
        # Convert to Epson graphics
        printer_data = ascii_to_epson_graphics(pixels, dpi, scale)
        
        # Write to file
        with open(output_prn, 'wb') as f:
            f.write(bytes(printer_data))
        
        print(f"Created printer file: {output_prn}")
        print(f"Send to printer with: cat {output_prn} > /dev/usblp0")
        print(f"Or on Windows: COPY /B {output_prn} LPT1:")
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description='Convert ASCII art (spaces and asterisks) to Epson LX-80 printer format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s ascii_art.txt output.prn
  %(prog)s logo.txt logo.prn --scale 2 --border
  %(prog)s art.txt art.prn --dpi 120

ASCII Art Format:
  Use asterisks (*) for black pixels
  Use spaces for white pixels
  Example file content:
    *****
    *   *
    *****
        """
    )
    
    parser.add_argument('input', help='Input ASCII art text file')
    parser.add_argument('output', help='Output PRN file')
    parser.add_argument('-d', '--dpi', choices=['60', '120'], default='60',
                        help='Graphics resolution (default: 60)')
    parser.add_argument('-s', '--scale', type=int, default=1,
                        help='Scaling factor (default: 1)')
    parser.add_argument('-b', '--border', action='store_true',
                        help='Add border around image')
    
    args = parser.parse_args()
    
    # Validate scale
    if args.scale < 1 or args.scale > 10:
        print("Error: Scale must be between 1 and 10")
        sys.exit(1)
    
    ascii_to_prn(args.input, args.output, args.dpi, args.scale, args.border)


if __name__ == '__main__':
    main()
