#!/usr/bin/env python3
"""
PNG to Epson LX-80 Printer File Converter
Converts PNG images to Epson ESC/P format for dot-matrix printing
"""

import sys
import argparse
from PIL import Image
import numpy as np


def convert_to_bw(image, threshold=128):
    """Convert image to black and white using threshold"""
    # Convert to grayscale
    gray = image.convert('L')
    # Apply threshold
    bw = gray.point(lambda x: 0 if x < threshold else 255, '1')
    return bw


def image_to_epson_graphics(image, dpi_mode='60'):
    """
    Convert PIL Image to Epson graphics bytes
    
    Args:
        image: PIL Image object (black and white)
        dpi_mode: '60' for ESC K (60 dpi), '120' for ESC L (120 dpi)
    
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
    
    width, height = image.size
    pixels = np.array(image)
    
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
                    # Check if pixel is black (0 in our BW image)
                    if pixels[y + bit, x] == 0:
                        byte_val |= (1 << (7 - bit))
            
            data.append(byte_val)
        
        # Line feed
        data.append(CR)
        data.append(LF)
    
    # Reset printer
    data.append(ESC)
    data.append(0x40)  # ESC @
        
    return data


def png_to_prn(input_png, output_prn, max_width=480, threshold=128, dpi='60'):
    """
    Convert PNG file to Epson LX-80 PRN file
    
    Args:
        input_png: Path to input PNG file
        output_prn: Path to output PRN file
        max_width: Maximum width in pixels (LX-80 can do ~480 pixels at 60dpi)
        threshold: Threshold for black/white conversion (0-255)
        dpi: '60' or '120' for graphics resolution
    """
    try:
        # Load image
        img = Image.open(input_png)
        
        # Resize if too wide, maintaining aspect ratio
        if img.width > max_width:
            ratio = max_width / img.width
            new_height = int(img.height * ratio)
            img = img.resize((max_width, new_height), Image.Resampling.LANCZOS)
            print(f"Resized to {max_width}x{new_height} pixels")
        
        # Convert to black and white
        bw_img = convert_to_bw(img, threshold)
        print(f"Converted to B&W (threshold={threshold})")
        print(f"Image size: {bw_img.width}x{bw_img.height} pixels")
        
        # Convert to Epson graphics
        printer_data = image_to_epson_graphics(bw_img, dpi)
        
        # Write to file
        with open(output_prn, 'wb') as f:
            f.write(bytes(printer_data))
        
        print(f"Created printer file: {output_prn}")
        print(f"Send to printer with: cat {output_prn} > /dev/usblp0")
        print(f"Or on Windows: COPY /B {output_prn} LPT1:")
        
    except FileNotFoundError:
        print(f"Error: Could not find input file '{input_png}'")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description='Convert PNG images to Epson LX-80 printer format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s input.png output.prn
  %(prog)s photo.png photo.prn --threshold 150 --width 400
  %(prog)s logo.png logo.prn --dpi 120
        """
    )
    
    parser.add_argument('input', help='Input PNG file')
    parser.add_argument('output', help='Output PRN file')
    parser.add_argument('-w', '--width', type=int, default=480,
                        help='Maximum width in pixels (default: 480)')
    parser.add_argument('-t', '--threshold', type=int, default=128,
                        help='B&W threshold 0-255 (default: 128)')
    parser.add_argument('-d', '--dpi', choices=['60', '120'], default='60',
                        help='Graphics resolution (default: 60)')
    
    args = parser.parse_args()
    
    # Validate threshold
    if not 0 <= args.threshold <= 255:
        print("Error: Threshold must be between 0 and 255")
        sys.exit(1)
    
    png_to_prn(args.input, args.output, args.width, args.threshold, args.dpi)


if __name__ == '__main__':
    main()
