#!/usr/bin/env python3

import sys
import os
import argparse
from typing import List, Tuple, Dict, Optional

class Material:
    """Class representing a material with properties."""
    def __init__(self, name: str):
        self.name = name
        self.diffuse_color = [1.0, 1,0, 1.0, 1.0]  # Default color (RGBA)
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
        if force_texture:
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
    def write_ps2_model(output_file: str, data: Dict, force_texture: str = None):
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
        PS2ModelGenerator._write_model_file(output_file, aligned_data, texture_file)
    
    @staticmethod
    def _write_model_file(output_file, data, texture_file):
        """Write the model data to a file with limited float precision."""
        # Define precision for floating point values
        # PS2 doesn't need more than 4-6 decimal places
        precision = 6
        
        with open(output_file, 'w') as f:
            # Write indices
            f.write(f"{len(data['indices']) * 3}\n")  # Total number of indices
            for idx in data['indices']:
                f.write(f"{idx[0]},{idx[1]},{idx[2]}\n")
            
            # Write aligned vertices with limited precision
            f.write(f"{len(data['vertices'])}\n")  # Vertex count
            for vertex in data['vertices']:
                # Format with limited precision
                vx = round(vertex[0], precision)
                vy = round(vertex[1], precision)
                vz = round(vertex[2], precision)
                vw = round(vertex[3], precision)
                f.write(f"{vx},{vy},{vz},{vw}\n")
            
            # Write aligned colors with limited precision
            f.write(f"{len(data['colors'])}\n")  # Color count
            for color in data['colors']:
                # Format with limited precision
                r = round(color[0], precision)
                g = round(color[1], precision)
                b = round(color[2], precision)
                a = round(color[3], precision)
                f.write(f"{r},{g},{b},{a}\n")
            
            # Write aligned texture coordinates with limited precision
            f.write(f"{len(data['texcoords'])}\n")  # Texcoord count
            for texcoord in data['texcoords']:
                # Ensure coordinates are in 0-1 range
                s = max(0.0, min(1.0, texcoord[0]))
                t = max(0.0, min(1.0, texcoord[1]))
                # Round to limited precision
                s = round(s, precision)
                t = round(t, precision)
                # Flip Y coordinate
                t_flipped = round(1.0 - t, precision)
                # Write to file with zeros for R and Q
                f.write(f"{s},{t_flipped},0.0,0.0\n")
            
            # Write texture data if available
            PS2ModelGenerator._write_texture_data(f, texture_file)
    
    @staticmethod
    def _write_texture_data(f, texture_file):
        """Write texture data to the model file."""
        if not texture_file or not os.path.exists(texture_file):
            return
            
        try:
            import PIL.Image as Image
            
            # Load the texture image
            img = Image.open(texture_file)
            width, height = img.size
            
            # Determine best texture format
            psm, bytes_per_pixel = PS2ModelGenerator.determine_texture_format(img)
            
            # Convert to appropriate mode based on PSM
            if psm == 0x00:  # GS_PSM_32
                img = img.convert('RGBA')
            elif psm == 0x01:  # GS_PSM_24
                img = img.convert('RGB')
            elif psm == 0x13:  # GS_PSM_8
                img = img.convert('L')
            
            # Calculate texture size in bytes
            texture_size = width * height * bytes_per_pixel
            
            # Write texture header (width, height, format, size)
            f.write(f"{width},{height},{psm},{texture_size}\n")
            
            # Write pixel data based on format
            pixels = img.load()
            PS2ModelGenerator._write_pixels(f, pixels, width, height, psm)
                
        except ImportError:
            print("Warning: PIL/Pillow library not found, skipping texture export")
            print("Install with: pip install pillow")
        except Exception as e:
            print(f"Error processing texture: {e}")
    
    @staticmethod
    def _write_pixels(f, pixels, width, height, psm):
        """Write pixel data based on texture format."""
        if psm == 0x00:  # GS_PSM_32 (32-bit RGBA)
            for y in range(height):
                for x in range(width):
                    r, g, b, a = pixels[x, y]
                    # RGBA packed as a hex value AARRGGBB
                    hex_value = (a << 24) | (r << 16) | (g << 8) | b
                    f.write(f"{hex_value:08x}\n")
                    
        elif psm == 0x01:  # GS_PSM_24 (24-bit RGB)
            for y in range(height):
                for x in range(width):
                    r, g, b = pixels[x, y]
                    # RGB packed as a hex value 00RRGGBB
                    hex_value = (r << 16) | (g << 8) | b
                    f.write(f"{hex_value:08x}\n")
                    
        elif psm == 0x13:  # GS_PSM_8 (8-bit grayscale)
            for y in range(height):
                for x in range(width):
                    l = pixels[x, y]
                    f.write(f"{l:02x}\n")

def main():
    parser = argparse.ArgumentParser(description='Convert OBJ files to PS2 custom format')
    parser.add_argument('input', help='Input OBJ file')
    parser.add_argument('output', help='Output PS2 model file')
    parser.add_argument('--force-texture', '-t', help='Force specific texture file (overrides MTL)')
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
    PS2ModelGenerator.write_ps2_model(args.output, model_data, args.force_texture)
    
    print(f"Conversion complete! Created {args.output}")
    print(f"  Vertices: {len(model_data['vertices'])}")
    print(f"  Texture coordinates: {len(model_data['texcoords'])}")
    print(f"  Triangles: {len(model_data['faces'])}")
    
    # Report on materials and textures
    texture_file = PS2ModelGenerator.find_texture_file(
        model_data['materials'], 
        args.output, 
        model_data['active_material'], 
        args.force_texture
    )
    
    if texture_file:
        print(f"  Using texture: {texture_file}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())