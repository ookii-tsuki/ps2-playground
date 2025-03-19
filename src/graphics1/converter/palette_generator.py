#!/usr/bin/env python3

import argparse
import os
import sys
from typing import List, Tuple
import numpy as np
from PIL import Image
import warnings

# PS2 GS PSM constants
GS_PSM_4 = 0x14  # 4-bit indexed
GS_PSM_8 = 0x13  # 8-bit indexed

class PaletteGenerator:
    """Extracts color palettes from multiple textures for PS2 CLUT textures."""
    
    def __init__(self, psm=GS_PSM_8, max_colors=None):
        """
        Initialize the palette generator.
        
        Args:
            psm: Either GS_PSM_4 (0x14) for 16 colors or GS_PSM_8 (0x13) for 256 colors
            max_colors: Override the default color count
        """
        self.psm = psm
        self.color_count = max_colors if max_colors else (16 if psm == GS_PSM_4 else 256)
        
        # Validate settings
        if psm not in [GS_PSM_4, GS_PSM_8]:
            raise ValueError(f"PSM must be either GS_PSM_4 (0x14) or GS_PSM_8 (0x13), got 0x{psm:02X}")
        
        if max_colors and (max_colors > self.color_count):
            warnings.warn(f"Requested {max_colors} colors but PSM 0x{psm:02X} only supports {self.color_count}")
            max_colors = self.color_count
    
    @staticmethod
    def load_images(file_paths: List[str]) -> List[Image.Image]:
        """Load multiple image files."""
        images = []
        for path in file_paths:
            if not os.path.exists(path):
                print(f"Warning: Image file {path} not found, skipping")
                continue
                
            try:
                img = Image.open(path).convert("RGBA")
                images.append(img)
                print(f"Loaded {path}: {img.width}x{img.height}, mode: {img.mode}")
            except Exception as e:
                print(f"Error loading {path}: {e}")
        
        if not images:
            raise ValueError("No valid images were loaded")
        
        return images
    
    def extract_palette(self, images: List[Image.Image]) -> List[Tuple[int, int, int, int]]:
        """
        Extract a palette from multiple images using color quantization.
        Returns a list of RGBA tuples.
        """
        # Combine all pixel data for analysis
        all_pixels = []
        for img in images:
            # Convert image to array of pixels
            pixels = np.array(img)
            # Reshape to a flat list of RGBA values
            pixels = pixels.reshape(-1, 4)
            all_pixels.append(pixels)
        
        # Combine all pixels
        combined_pixels = np.vstack(all_pixels)
        
        # Use K-means clustering to find dominant colors
        from sklearn.cluster import KMeans
        
        # Sample pixels if there are too many (for performance)
        max_samples = 100000  # Limit samples for faster processing
        if len(combined_pixels) > max_samples:
            indices = np.random.choice(len(combined_pixels), max_samples, replace=False)
            sample_pixels = combined_pixels[indices]
        else:
            sample_pixels = combined_pixels
            
        # Apply K-means to find dominant colors
        print(f"Clustering to find {self.color_count} dominant colors...")
        kmeans = KMeans(n_clusters=self.color_count, random_state=42, n_init=10)
        kmeans.fit(sample_pixels)
        
        # Get the colors from cluster centers
        colors = kmeans.cluster_centers_.astype(int)
        
        # Ensure colors are in valid range (0-255)
        colors = np.clip(colors, 0, 255)
        
        # Convert to list of RGBA tuples
        palette = [tuple(color) for color in colors]
        
        # Ensure the first color is transparent black (for common transparency usage)
        # Only do this if we have a color with alpha < 128
        has_transparent = any(color[3] < 128 for color in palette)
        if has_transparent and (0, 0, 0, 0) not in palette:
            palette[0] = (0, 0, 0, 0)
        
        return palette
    
    def generate_palette_image(self, palette: List[Tuple[int, int, int, int]], 
                              width: int = 16) -> Image.Image:
        """Generate a visual representation of the palette as an image."""
        # Calculate height based on color count and width
        height = (self.color_count + width - 1) // width
        
        # Create a new image with the palette
        palette_img = Image.new('RGBA', (width, height))
        pixels = palette_img.load()
        
        # Fill the image with palette colors
        for i, color in enumerate(palette):
            if i >= self.color_count:
                break
            x = i % width
            y = i // width
            pixels[x, y] = color
            
        return palette_img
    
    def save_ps2_clut(self, palette: List[Tuple[int, int, int, int]], output_path: str) -> None:
        """Save the palette in PS2 CLUT format."""
        # Write header
        with open(output_path, 'wb') as f:
            # Write header
            f.write(b'CLT\0')  # Magic identifier
            f.write(self.psm.to_bytes(1, byteorder='little'))  # PSM value
            f.write(len(palette).to_bytes(2, byteorder='little'))  # Color count
            
            # Write color data in PS2 CLUT format (ABGR8888)
            for color in palette:
                r, g, b, a = color
                # Convert to PS2 32-bit RGBA format
                f.write(bytes([r, g, b, a]))
        
        psm_name = "4-bit" if self.psm == GS_PSM_4 else "8-bit" if self.psm == GS_PSM_8 else f"0x{self.psm:02X}"
        print(f"Saved PS2 CLUT with {len(palette)} colors ({psm_name}) to {output_path}")
    
    def save_preview_image(self, palette: List[Tuple[int, int, int, int]], output_path: str) -> None:
        """Save a preview image of the palette."""
        preview_path = output_path.rsplit('.', 1)[0] + '.png'
        palette_img = self.generate_palette_image(palette)
        
        # Resize for better visibility
        scale = 16
        preview = palette_img.resize(
            (palette_img.width * scale, palette_img.height * scale),
            resample=Image.NEAREST
        )
        preview.save(preview_path)
        
        print(f"Saved palette preview to {preview_path}")


def main():
    parser = argparse.ArgumentParser(description='Generate PS2 CLUT textures from multiple images')
    parser.add_argument('input_files', nargs='+', help='Input texture files')
    parser.add_argument('output_file', help='Output CLUT file (.clt)')
    parser.add_argument('--psm', '-p', type=lambda x: int(x, 0), 
                        choices=[GS_PSM_4, GS_PSM_8], default=GS_PSM_8,
                        help='PSM format: 0x14 (4-bit) or 0x13 (8-bit)')
    parser.add_argument('--colors', '-c', type=int, help='Override the number of colors')
    parser.add_argument('--preview', '-v', action='store_true', 
                        help='Generate a preview image of the palette')
    args = parser.parse_args()
    
    try:
        # Initialize palette generator
        generator = PaletteGenerator(args.psm, args.colors)
        
        # Load input images
        print(f"Loading {len(args.input_files)} texture files...")
        images = PaletteGenerator.load_images(args.input_files)
        
        # Extract palette
        palette = generator.extract_palette(images)
        
        # Ensure output directory exists
        os.makedirs(os.path.dirname(os.path.abspath(args.output_file)), exist_ok=True)
        
        # Set proper extension if needed
        if not args.output_file.lower().endswith('.clt'):
            output_file = args.output_file + '.clt'
        else:
            output_file = args.output_file
            
        # Save PS2 CLUT format
        generator.save_ps2_clut(palette, output_file)
        
        # Generate preview if requested
        if args.preview:
            generator.save_preview_image(palette, output_file)
        
        print("PS2 CLUT generation complete!")
        return 0
    
    except Exception as e:
        print(f"Error: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())