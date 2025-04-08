#!/usr/bin/env python3

import sys
import os
import argparse
from typing import List, Tuple, Dict, Optional
import struct

class Material:
    """Class representing a material with properties."""
    def __init__(self, name: str):
        self.name = name
        self.diffuse_color = [1.0, 1.0, 1.0, 1.0]  # Default color (RGBA)
        self.texture_map = None  # Path to texture file

    def set_diffuse_color(self, r: float, g: float, b: float, a: float = 1.0):
        self.diffuse_color = [r, g, b, a]

    def set_transparency(self, alpha: float):
        self.diffuse_color[3] = alpha

    def set_texture(self, texture_path: str):
        self.texture_map = texture_path

class ObjParser:
    """Parses OBJ and MTL files into memory structures."""
    
    @staticmethod
    def parse_mtl_file(mtl_file: str) -> Dict[str, Material]:
        """Parse an MTL file and extract material properties."""
        materials = {}
        current_material = None
        
        if not os.path.exists(mtl_file):
            print(f"Warning: MTL file {mtl_file} not found")
            return materials
        
        with open(mtl_file, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                
                values = line.split()
                if not values:
                    continue
                    
                if values[0] == 'newmtl':
                    # Start a new material
                    current_material = Material(values[1])
                    materials[values[1]] = current_material
                
                elif current_material is not None:
                    if values[0] == 'Kd':  # Diffuse color
                        # Convert RGB to RGBA (alpha=1.0)
                        current_material.set_diffuse_color(
                            float(values[1]), 
                            float(values[2]), 
                            float(values[3])
                        )
                        
                    elif values[0] == 'd' or values[0] == 'Tr':  # Transparency
                        # 'd' is dissolve factor, 'Tr' is transparency
                        alpha = float(values[1])
                        if values[0] == 'Tr':  # Convert transparency to alpha
                            alpha = 1.0 - alpha
                        current_material.set_transparency(alpha)
                        
                    elif values[0] == 'map_Kd':  # Diffuse texture map
                        texture_path = ' '.join(values[1:])
                        current_material.set_texture(texture_path)
        
        return materials
    
    @staticmethod
    def parse_obj_file(obj_file: str) -> Dict:
        """Parse an OBJ file and extract model data."""
        vertices = []
        texcoords = []
        normals = []
        faces = []
        materials = {}
        active_material = None
        material_lib = None
        
        with open(obj_file, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                    
                values = line.split()
                if not values:
                    continue
                    
                if values[0] == 'v':  # Vertex
                    # Add 1.0 as w coordinate if not present
                    v = [float(values[1]), float(values[2]), float(values[3])]
                    if len(values) > 4:
                        v.append(float(values[4]))
                    else:
                        v.append(1.0)
                    vertices.append(v)
                    
                elif values[0] == 'vt':  # Texture coordinate
                    # Add 0.0 as r,q coordinates if not present
                    vt = [float(values[1]), float(values[2])]
                    if len(values) > 3:
                        vt.append(float(values[3]))
                    else:
                        vt.append(0.0)
                    if len(values) > 4:
                        vt.append(float(values[4]))
                    else:
                        vt.append(0.0)
                    texcoords.append(vt)
                    
                elif values[0] == 'vn':  # Normal
                    normals.append([float(values[1]), float(values[2]), float(values[3])])
                    
                elif values[0] == 'f':  # Face
                    ObjParser._parse_face(values, faces, active_material)
                
                elif values[0] == 'mtllib':
                    # Material library
                    material_lib = ' '.join(values[1:])
                    
                elif values[0] == 'usemtl':
                    # Use material
                    active_material = values[1]
        
        # Parse material file if available
        if material_lib:
            mtl_path = os.path.join(os.path.dirname(obj_file), material_lib)
            materials = ObjParser.parse_mtl_file(mtl_path)
        
        return {
            'vertices': vertices,
            'texcoords': texcoords, 
            'normals': normals,
            'faces': faces,
            'materials': materials,
            'active_material': active_material
        }
    
    @staticmethod
    def _parse_face(values, faces, active_material):
        """Parse face data from OBJ file."""
        # Face indices are in format: vertex_idx/texcoord_idx/normal_idx
        # Note: OBJ indices are 1-based, we'll convert to 0-based
        face_vertices = []
        face_texcoords = []
        face_normals = []
        
        for v in values[1:]:
            # Handle different face formats
            if '/' in v:
                parts = v.split('/')
                # Get vertex index (required)
                face_vertices.append(int(parts[0]) - 1 if parts[0] else -1)
                
                # Get texcoord index (optional)
                if len(parts) > 1 and parts[1]:
                    face_texcoords.append(int(parts[1]) - 1)
                else:
                    face_texcoords.append(-1)
                
                # Get normal index (optional)
                if len(parts) > 2 and parts[2]:
                    face_normals.append(int(parts[2]) - 1)
                else:
                    face_normals.append(-1)
            else:
                # Only vertex indices
                face_vertices.append(int(v) - 1)
                face_texcoords.append(-1)
                face_normals.append(-1)
        
        # Triangulate faces with more than 3 vertices
        if len(face_vertices) == 3:
            # Already a triangle
            faces.append({
                'vertices': face_vertices,
                'texcoords': face_texcoords,
                'normals': face_normals,
                'material': active_material
            })
        else:
            # Triangulate (fan triangulation)
            for i in range(1, len(face_vertices) - 1):
                faces.append({
                    'vertices': [face_vertices[0], face_vertices[i], face_vertices[i + 1]],
                    'texcoords': [face_texcoords[0], face_texcoords[i], face_texcoords[i + 1]],
                    'normals': [face_normals[0], face_normals[i], face_normals[i + 1]],
                    'material': active_material
                })

class PS2ModelGenerator:
    """Generates PS2-compatible model files from parsed OBJ data."""
    
    @staticmethod
    def generate_colors(vertex_count: int, materials: Dict, faces: List[Dict]) -> List[List[float]]:
        """Generate colors for vertices based on material assignments."""
        # Start with default white colors
        colors = [[1.0, 1.0, 1.0, 1.0] for _ in range(vertex_count)]
        
        # Track which vertices have already been assigned a color
        assigned = [False] * vertex_count
        
        # Assign colors based on face materials
        for face in faces:
            material_name = face['material']
            if material_name and material_name in materials:
                material = materials[material_name]
                color = material.diffuse_color
                
                # Assign color to each vertex in the face
                for vertex_idx in face['vertices']:
                    if vertex_idx >= 0 and not assigned[vertex_idx]:
                        colors[vertex_idx] = color.copy()
                        assigned[vertex_idx] = True
        
        return colors

    @staticmethod
    def determine_texture_format(img):
        """Determine the best texture format for the image."""
        if img.mode == 'RGBA':
            return 0x00, 4  # GS_PSM_32 (32-bit)
        elif img.mode == 'RGB':
            return 0x01, 3  # GS_PSM_24 (24-bit)
        elif img.mode == 'LA' or img.mode == 'L':
            return 0x13, 1  # GS_PSM_8 (8-bit)
        else:
            # Default to 32-bit if unsure
            print(f"Warning: Unsupported image mode {img.mode}, defaulting to 32-bit")
            return 0x00, 4

    @staticmethod
    def align_vertex_data(vertices, texcoords, colors, faces):
        """Align vertices with their corresponding texture coordinates."""
        # Create new arrays with proper alignment
        aligned_vertices = []
        aligned_colors = []
        aligned_texcoords = []
        new_indices = []
        
        # Map from (vertex_idx, texcoord_idx) to new aligned index
        vertex_texcoord_map = {}
        
        # Process each face to build the new aligned arrays
        for face in faces:
            face_new_indices = []
            
            for i in range(3):  # Each face is a triangle
                v_idx = face['vertices'][i]
                t_idx = face['texcoords'][i]
                
                # Skip if we don't have valid indices
                if v_idx < 0 or t_idx < 0:
                    # Use a default mapping if missing
                    if v_idx >= 0:
                        # We have vertex but no texcoord
                        key = (v_idx, -1)
                        if key not in vertex_texcoord_map:
                            new_idx = len(aligned_vertices)
                            vertex_texcoord_map[key] = new_idx
                            aligned_vertices.append(vertices[v_idx])
                            aligned_colors.append(colors[v_idx])
                            # Use a default texcoord (e.g., top-left corner of texture)
                            aligned_texcoords.append([0.0, 0.0, 0.0, 0.0])
                        face_new_indices.append(vertex_texcoord_map[key])
                    continue
                
                key = (v_idx, t_idx)
                
                if key not in vertex_texcoord_map:
                    # Create a new aligned vertex with this specific texcoord
                    new_idx = len(aligned_vertices)
                    vertex_texcoord_map[key] = new_idx
                    
                    # Copy vertex data
                    aligned_vertices.append(vertices[v_idx])
                    aligned_colors.append(colors[v_idx])
                    
                    # Copy texcoord data
                    if t_idx < len(texcoords):
                        aligned_texcoords.append(texcoords[t_idx])
                    else:
                        print(f"Warning: Texcoord index {t_idx} out of range")
                        aligned_texcoords.append([0.0, 0.0, 0.0, 0.0])
                
                # Add to the new face
                face_new_indices.append(vertex_texcoord_map[key])
            
            # Only add faces that have 3 valid indices
            if len(face_new_indices) == 3:
                new_indices.append(face_new_indices)
        
        return {
            'vertices': aligned_vertices,
            'colors': aligned_colors,
            'texcoords': aligned_texcoords,
            'indices': new_indices
        }

    @staticmethod
    def find_texture_file(materials, output_file, active_material=None, force_texture=None):
        """Find the appropriate texture file to use."""
        if (force_texture):
            return force_texture if os.path.exists(force_texture) else None
        
        texture_file = None
        for material_name, material in materials.items():
            if material.texture_map and (not texture_file or material_name == active_material):
                texture_path = material.texture_map
                # Check if the texture exists
                if os.path.exists(texture_path):
                    texture_file = texture_path
                    break
                # Try relative path
                base_path = os.path.dirname(output_file)
                rel_path = os.path.join(base_path, os.path.basename(texture_path))
                if os.path.exists(rel_path):
                    texture_file = rel_path
                    break
        
        return texture_file

    @staticmethod
    def write_ps2_model(output_file: str, data: Dict, force_texture: str = None, clut_file: str = None):
        """Write the converted model to a PS2-compatible format."""
        vertices = data['vertices']
        texcoords = data['texcoords']
        colors = data['colors']
        faces = data['faces']
        materials = data['materials']
        active_material = data['active_material']
        
        # Find texture file
        texture_file = PS2ModelGenerator.find_texture_file(
            materials, output_file, active_material, force_texture)
        
        # Align vertices with texture coordinates
        aligned_data = PS2ModelGenerator.align_vertex_data(
            vertices, texcoords, colors, faces)
        
        # Print diagnostics
        print(f"Original vertices: {len(vertices)}, Aligned vertices: {len(aligned_data['vertices'])}")
        print(f"Original texcoords: {len(texcoords)}, Aligned texcoords: {len(aligned_data['texcoords'])}")
        print(f"Final triangles: {len(aligned_data['indices'])}")
        
        # Write the model file
        PS2ModelGenerator._write_model_file(output_file, aligned_data, texture_file, clut_file)
    
    @staticmethod
    def _write_model_file(output_file, data, texture_file, clut_file=None):
        """Write the model data to a binary file format."""
        # Convert triangles to strips
        triangle_strips = PS2ModelGenerator.triangles_to_strips(data['indices'])
        
        with open(output_file, 'wb') as f:
            # Write number of strips (4-byte unsigned int)
            f.write(len(triangle_strips).to_bytes(4, byteorder='little'))
            
            # Write each strip
            for strip in triangle_strips:
                # Write strip length (4-byte unsigned int)
                f.write(len(strip).to_bytes(4, byteorder='little'))
                # Write indices (4-byte unsigned ints)
                for idx in strip:
                    f.write(idx.to_bytes(4, byteorder='little'))
            
            # Write aligned vertices
            f.write(len(data['vertices']).to_bytes(4, byteorder='little'))  # Vertex count
            for vertex in data['vertices']:
                # Write 4 floating-point values (x,y,z,w)
                for value in vertex:
                    f.write(struct.pack('<f', float(value)))
            
            # Write aligned colors
            f.write(len(data['colors']).to_bytes(4, byteorder='little'))  # Color count
            for color in data['colors']:
                # Write 4 floating-point values (r,g,b,a)
                for value in color:
                    f.write(struct.pack('<f', float(value)))
                    print(f"Val: {value}")
                print(f"Color: {color}")
            
            # Write aligned texture coordinates
            f.write(len(data['texcoords']).to_bytes(4, byteorder='little'))  # Texcoord count
            for texcoord in data['texcoords']:
                # Ensure coordinates are in 0-1 range
                s = max(0.0, min(1.0, texcoord[0]))
                t = max(0.0, min(1.0, texcoord[1]))
                # Flip Y coordinate for PS2
                t_flipped = 1.0 - t
                # Write as 4 floating point values (s,t,0,0)
                f.write(struct.pack('<f', s))
                f.write(struct.pack('<f', t_flipped))
                f.write(struct.pack('<f', 0.0))
                f.write(struct.pack('<f', 0.0))
            
            # Write texture data if available
            PS2ModelGenerator._write_texture_data_binary(f, texture_file, clut_file)

    @staticmethod
    def _write_texture_data_binary(f, texture_file, clut_file=None):
        """Write texture data to the model file in binary format."""
        if not texture_file or not os.path.exists(texture_file):
            # No texture - write 0 for width and height to indicate no texture
            f.write((0).to_bytes(4, byteorder='little'))  # Width = 0
            f.write((0).to_bytes(4, byteorder='little'))  # Height = 0
            return
            
        try:
            import PIL.Image as Image
            import numpy as np
            import struct
            
            # Load the texture image
            img = Image.open(texture_file)
            width, height = img.size
            
            # Initialize CLUT ID as 0 (no CLUT)
            clut_id = 0
            
            # Check if we're using a CLUT
            if (clut_file and os.path.exists(clut_file)):
                # Load CLUT file
                palette, psm, loaded_clut_id = PS2ModelGenerator._load_clut_file(clut_file)
                if palette is None:
                    # Fall back to non-indexed if CLUT loading fails
                    print(f"Warning: Failed to load CLUT, falling back to direct color")
                    clut_file = None
                    psm, bytes_per_pixel = PS2ModelGenerator.determine_texture_format(img)
                else:
                    # Save the CLUT ID
                    clut_id = loaded_clut_id
                    # Calculate bytes per pixel based on PSM
                    bytes_per_pixel = 1 if psm == 0x13 else 0.5  # 8-bit (1 byte) or 4-bit (0.5 byte)
                    print(f"Using CLUT with PSM=0x{psm:02X}, ID=0x{clut_id:08X}, {len(palette)} colors")
            else:
                # No CLUT, use direct color format
                psm, bytes_per_pixel = PS2ModelGenerator.determine_texture_format(img)
            
            # Convert to appropriate mode based on PSM
            if clut_file and (psm == 0x14 or psm == 0x13):  # Indexed modes
                img = img.convert('RGBA')  # Keep as RGBA for color matching
            elif psm == 0x00:  # GS_PSM_32
                img = img.convert('RGBA')
            elif psm == 0x01:  # GS_PSM_24
                img = img.convert('RGB')
            elif psm == 0x13:  # GS_PSM_8 (grayscale without CLUT)
                img = img.convert('L')
            
            # Calculate texture size in bytes
            texture_size = int(width * height * bytes_per_pixel)
            
            # Write texture header
            f.write(width.to_bytes(4, byteorder='little'))          # Width (uint32)
            f.write(height.to_bytes(4, byteorder='little'))         # Height (uint32)
            f.write(psm.to_bytes(4, byteorder='little'))            # PSM format (uint32)
            f.write(texture_size.to_bytes(4, byteorder='little'))   # Texture size (uint32)
            f.write(clut_id.to_bytes(4, byteorder='little'))        # CLUT ID (uint32)
            
            # Write pixel data based on format
            pixels = img.load()
            
            if clut_file and palette is not None:
                # Write indexed texture with CLUT in memory
                PS2ModelGenerator._write_pixels_indexed_binary(f, pixels, width, height, palette, psm)
            else:
                # Standard direct color texture
                PS2ModelGenerator._write_pixels_binary(f, pixels, width, height, psm)
                
        except ImportError:
            print("Warning: PIL/Pillow library not found, skipping texture export")
            print("Install with: pip install pillow")
            # Write zeros to indicate no texture
            f.write((0).to_bytes(4, byteorder='little'))  # Width = 0
            f.write((0).to_bytes(4, byteorder='little'))  # Height = 0
        except Exception as e:
            print(f"Error processing texture: {e}")
            import traceback
            traceback.print_exc()
            # Write zeros to indicate no texture
            f.write((0).to_bytes(4, byteorder='little'))  # Width = 0
            f.write((0).to_bytes(4, byteorder='little'))  # Height = 0

    @staticmethod
    def _write_pixels_indexed_binary(f, pixels, width, height, palette, psm):
        """Write indexed pixel data for CLUT textures in binary format."""
        # Convert palette to numpy array for faster color matching
        import numpy as np
        palette_array = np.array(palette)
        
        if psm == 0x14:  # GS_PSM_4 (4-bit indexed, 16 colors)
            # Process pairs of pixels to create bytes (4-bits per pixel)
            for y in range(height):
                for x in range(0, width, 2):
                    # Process two pixels at a time to create one byte
                    indices = []
                    
                    for dx in range(2):
                        if x + dx < width:
                            # Get pixel color
                            pixel_x = x + dx
                            if len(pixels[pixel_x, y]) == 4:
                                r, g, b, a = pixels[pixel_x, y]
                            else:
                                r, g, b = pixels[pixel_x, y]
                                a = 255
                            
                            # Find closest color in palette
                            pixel_color = np.array([r, g, b, a])
                            color_distances = np.sqrt(np.sum((palette_array - pixel_color) ** 2, axis=1))
                            closest_idx = np.argmin(color_distances)
                            indices.append(closest_idx)
                        else:
                            # Padding for odd width
                            indices.append(0)
                    
                    # Pack two 4-bit indices into one byte
                    byte_value = (indices[0] << 4) | (indices[1] & 0x0F)
                    f.write(bytes([byte_value]))
        
        elif psm == 0x13:  # GS_PSM_8 (8-bit indexed, 256 colors)
            for y in range(height):
                for x in range(width):
                    # Get pixel color
                    if len(pixels[x, y]) == 4:
                        r, g, b, a = pixels[x, y]
                    else:
                        r, g, b = pixels[x, y]
                        a = 255
                    
                    # Find closest color in palette
                    pixel_color = np.array([r, g, b, a])
                    color_distances = np.sqrt(np.sum((palette_array - pixel_color) ** 2, axis=1))
                    closest_idx = np.argmin(color_distances)
                    
                    # Write one byte per pixel (8-bit index)
                    f.write(bytes([closest_idx]))

    @staticmethod
    def _write_pixels_binary(f, pixels, width, height, psm):
        """Write pixel data based on texture format in binary."""
        if psm == 0x00:  # GS_PSM_32 (32-bit RGBA)
            for y in range(height):
                for x in range(width):
                    r, g, b, a = pixels[x, y]
                    # Write RGBA bytes in little-endian order (ABGR)
                    f.write(bytes([r, g, b, a]))
                    
        elif psm == 0x01:  # GS_PSM_24 (24-bit RGB)
            for y in range(height):
                for x in range(width):
                    r, g, b = pixels[x, y]
                    # Write RGB bytes in little-endian order (BGR)
                    f.write(bytes([r, g, b]))
                    
        elif psm == 0x13:  # GS_PSM_8 (8-bit grayscale)
            for y in range(height):
                for x in range(width):
                    l = pixels[x, y]
                    # Write single byte for grayscale
                    f.write(bytes([l]))

    @staticmethod
    def _load_clut_file(clut_file):
        """Load a CLUT file and extract the palette and ID."""
        try:
            with open(clut_file, 'rb') as f:
                # Read and verify magic identifier
                magic = f.read(4)
                if magic != b'CLT\0':
                    print(f"Error: Invalid CLUT file format (bad magic number)")
                    return None, None, None
                
                # Read PSM
                psm = int.from_bytes(f.read(1), byteorder='little')
                if psm != 0x14 and psm != 0x13:
                    print(f"Error: Invalid CLUT PSM (must be 0x14 or 0x13, got 0x{psm:02X})")
                    return None, None, None
                
                # Read color count
                color_count = int.from_bytes(f.read(2), byteorder='little')
                
                # Read CLUT ID
                clut_id = int.from_bytes(f.read(4), byteorder='little')
                print(f"Found CLUT ID: 0x{clut_id:08X}")
                
                # Validate color count
                max_colors = 16 if psm == 0x14 else 256
                if color_count > max_colors:
                    print(f"Warning: CLUT color count ({color_count}) exceeds maximum for PSM 0x{psm:02X} ({max_colors})")
                    color_count = max_colors
                
                # Read color data
                palette = []
                for i in range(color_count):
                    color_bytes = f.read(4)
                    if len(color_bytes) != 4:
                        print(f"Error: Failed to read CLUT color data at index {i}")
                        return None, None, None
                    
                    r, g, b, a = color_bytes
                    palette.append((r, g, b, a))
                
                return palette, psm, clut_id
        except Exception as e:
            print(f"Error loading CLUT file: {e}")
            import traceback
            traceback.print_exc()
            return None, None, None

    @staticmethod
    def save_reconstructed_texture(texture_file, clut_file, output_path=None):
        """
        Reconstruct a texture using CLUT palette and save for debugging purposes.
        
        Args:
            texture_file: Path to original texture
            clut_file: Path to CLUT file
            output_path: Path to save reconstructed texture (defaults to original_texture_reconstructed.png)
        
        Returns:
            Path to saved reconstructed texture or None if failed
        """
        try:
            import PIL.Image as Image
            import numpy as np
            
            if not output_path:
                base_name = os.path.splitext(texture_file)[0]
                output_path = f"{base_name}_reconstructed.png"
            
            # Load the original texture
            original_img = Image.open(texture_file)
            width, height = original_img.size
            
            # Convert to RGBA for consistent color comparison
            original_img = original_img.convert('RGBA')
            original_pixels = original_img.load()
            
            # Load CLUT file
            palette, psm, clut_id = PS2ModelGenerator._load_clut_file(clut_file)
            if palette is None:
                print(f"Error: Failed to load CLUT for reconstruction")
                return None
            
            print(f"Reconstructing texture using CLUT ID: 0x{clut_id:08X}")
            
            # Create a new image for the reconstruction
            reconstructed_img = Image.new('RGBA', (width, height))
            reconstructed_pixels = reconstructed_img.load()
            
            # Convert palette to numpy array for faster color matching
            palette_array = np.array(palette)
            
            # Process each pixel
            for y in range(height):
                for x in range(width):
                    # Get original pixel color
                    r, g, b, a = original_pixels[x, y]
                    
                    # Find closest color in palette
                    pixel_color = np.array([r, g, b, a])
                    color_distances = np.sqrt(np.sum((palette_array - pixel_color) ** 2, axis=1))
                    closest_idx = np.argmin(color_distances)
                    
                    # Use the palette color for reconstruction
                    reconstructed_pixels[x, y] = palette[closest_idx]
            
            # Save the reconstructed image
            reconstructed_img.save(output_path)
            print(f"Saved reconstructed texture to {output_path}")
            
            # Optionally create a side-by-side comparison
            comparison_path = f"{base_name}_comparison.png"
            comparison_img = Image.new('RGBA', (width * 2, height))
            comparison_img.paste(original_img, (0, 0))
            comparison_img.paste(reconstructed_img, (width, 0))
            comparison_img.save(comparison_path)
            print(f"Saved side-by-side comparison to {comparison_path}")
            
            return output_path
            
        except ImportError:
            print("Warning: PIL/Pillow library not found, cannot reconstruct texture")
            print("Install with: pip install pillow")
            return None
        except Exception as e:
            print(f"Error reconstructing texture: {e}")
            import traceback
            traceback.print_exc()
            return None

    @staticmethod
    def triangles_to_strips(indices):
        """
        Convert triangle list to triangle strips.
        
        Args:
            indices: List of triangle indices, each containing [v1, v2, v3]
        
        Returns:
            List of triangle strips, each containing a list of vertex indices
        """
        # Edge map for finding adjacent triangles
        edges = {}
        triangles = set(range(len(indices)))
        used_triangles = set()
        
        # Build edge connectivity map
        for i, tri in enumerate(indices):
            for j in range(3):
                # Create directed edge (v1, v2)
                v1 = tri[j]
                v2 = tri[(j + 1) % 3]
                edge = (v1, v2)
                
                if edge not in edges:
                    edges[edge] = []
                edges[edge].append(i)
                
                # Also add reversed edge for easier adjacency check
                reversed_edge = (v2, v1)
                if reversed_edge not in edges:
                    edges[reversed_edge] = []
                edges[reversed_edge].append(i)
        
        strips = []
        
        # While we have unused triangles
        while triangles - used_triangles:
            # Start a new strip
            current_strip = []
            
            # Pick an unused triangle to start the strip
            tri_idx = next(iter(triangles - used_triangles))
            used_triangles.add(tri_idx)
            
            # Add the first triangle to the strip
            first_tri = indices[tri_idx]
            current_strip.extend([first_tri[0], first_tri[1], first_tri[2]])
            
            # Try to extend the strip
            can_extend = True
            while (can_extend):
                can_extend = False
                
                # Get the last two vertices of the strip
                v1, v2 = current_strip[-2], current_strip[-1]
                
                # Look for a triangle that shares the edge (v2, v1)
                edge = (v2, v1)
                if edge in edges:
                    for adj_tri_idx in edges[edge]:
                        if adj_tri_idx in triangles and adj_tri_idx not in used_triangles:
                            # Found adjacent triangle, add its third vertex to the strip
                            adj_tri = indices[adj_tri_idx]
                            
                            # Find the vertex that is not part of the edge
                            for v in adj_tri:
                                if v != v1 and v != v2:
                                    current_strip.append(v)
                                    used_triangles.add(adj_tri_idx)
                                    can_extend = True
                                    break
                            break
                
                # If we couldn't extend one way, try the other direction
                if not can_extend and len(current_strip) >= 3:
                    v1, v2 = current_strip[1], current_strip[0]
                    edge = (v1, v2)
                    
                    if edge in edges:
                        for adj_tri_idx in edges[edge]:
                            if adj_tri_idx in triangles and adj_tri_idx not in used_triangles:
                                # Found adjacent triangle, prepend its third vertex to the strip
                                adj_tri = indices[adj_tri_idx]
                                
                                # Find the vertex that is not part of the edge
                                for v in adj_tri:
                                    if v != v1 and v != v2:
                                        current_strip.insert(0, v)
                                        used_triangles.add(adj_tri_idx)
                                        can_extend = True
                                        break
                                break
            
            # Add the strip if it's valid (at least 3 vertices)
            if len(current_strip) >= 3:
                strips.append(current_strip)
        
        return strips

    @staticmethod
    def calculate_strip_statistics(triangle_strips):
        """Calculate statistics about the generated triangle strips."""
        if not triangle_strips:
            return {
                'strip_count': 0,
                'total_indices': 0,
                'avg_strip_length': 0,
                'min_strip_length': 0,
                'max_strip_length': 0,
                'compression_ratio': 0
            }
        
        strip_lengths = [len(strip) for strip in triangle_strips]
        total_indices = sum(strip_lengths)
        min_length = min(strip_lengths)
        max_length = max(strip_lengths)
        avg_length = total_indices / len(triangle_strips)
        
        # Each triangle in a strip with n vertices has (n-2) triangles
        strip_triangles = sum(max(0, len(strip) - 2) for strip in triangle_strips)
        
        # Compare to original triangle count (3 indices per triangle)
        original_triangle_count = total_indices // 3
        compression_ratio = 1.0
        if original_triangle_count > 0:
            compression_ratio = strip_triangles / original_triangle_count
        
        return {
            'strip_count': len(triangle_strips),
            'total_indices': total_indices,
            'avg_strip_length': avg_length,
            'min_strip_length': min_length,
            'max_strip_length': max_length,
            'triangles_in_strips': strip_triangles,
            'compression_ratio': compression_ratio
        }

def main():
    parser = argparse.ArgumentParser(description='Convert OBJ files to PS2 custom format')
    parser.add_argument('input', help='Input OBJ file')
    parser.add_argument('output', help='Output PS2 model file')
    parser.add_argument('--force-texture', '-t', help='Force specific texture file (overrides MTL)')
    parser.add_argument('--clut', '-c', help='Use specified CLUT file for indexed textures')
    parser.add_argument('--debug', '-d', action='store_true', help='Enable debug output')
    args = parser.parse_args()
    
    if not os.path.exists(args.input):
        print(f"Error: Input file {args.input} does not exist")
        return 1
    
    print(f"Converting {args.input} to {args.output}...")
    
    # Parse the OBJ file
    model_data = ObjParser.parse_obj_file(args.input)
    
    # Generate colors based on materials
    model_data['colors'] = PS2ModelGenerator.generate_colors(
        len(model_data['vertices']), 
        model_data['materials'], 
        model_data['faces']
    )
    
    # Create the PS2 model file
    PS2ModelGenerator.write_ps2_model(args.output, model_data, args.force_texture, args.clut)
    
    print(f"Conversion complete! Created {args.output}")
    print(f"  Vertices: {len(model_data['vertices'])}")
    print(f"  Texture coordinates: {len(model_data['texcoords'])}")
    
    # Calculate triangle strip statistics
    triangle_strips = PS2ModelGenerator.triangles_to_strips(
        PS2ModelGenerator.align_vertex_data(
            model_data['vertices'], 
            model_data['texcoords'],
            model_data['colors'], 
            model_data['faces']
        )['indices']
    )
    stats = PS2ModelGenerator.calculate_strip_statistics(triangle_strips)
    
    print(f"  Triangle strips: {stats['strip_count']}")
    print(f"  Avg strip length: {stats['avg_strip_length']:.2f} vertices")
    print(f"  Original triangles: {len(model_data['faces'])}")
    print(f"  Triangles in strips: {stats['triangles_in_strips']}")
    print(f"  Compression ratio: {stats['compression_ratio']:.2f}")
    
    # Report on materials and textures
    texture_file = PS2ModelGenerator.find_texture_file(
        model_data['materials'], 
        args.output, 
        model_data['active_material'], 
        args.force_texture
    )
    
    if texture_file:
        print(f"  Using texture: {texture_file}")
        
        # If debugging is enabled and we have a CLUT file, reconstruct the texture
        if args.debug and args.clut and os.path.exists(args.clut):
            print("Debug: Reconstructing texture from CLUT palette...")
            PS2ModelGenerator.save_reconstructed_texture(texture_file, args.clut)
    
    return 0

if __name__ == "__main__":
    sys.exit(main())