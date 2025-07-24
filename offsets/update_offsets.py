#!/usr/bin/env python3
"""
CS2 Offsets Converter
Converts hex offsets from offsets.hpp to decimal and updates offsets.json
"""

import re
import json
from pathlib import Path
from typing import Dict, Any

class CS2OffsetsConverter:
    def __init__(self):
        self.offsets = {}
        
    def parse_hpp_file(self, hpp_file_path: str) -> Dict[str, int]:
        """Parse the offsets.hpp file and extract hex values, converting to decimal"""
        offsets = {}
        
        try:
            with open(hpp_file_path, 'r', encoding='utf-8') as file:
                content = file.read()
                
            # Pattern to match constexpr lines with hex values
            pattern = r'constexpr\s+std::ptrdiff_t\s+(\w+)\s+=\s+(0x[0-9A-Fa-f]+);'
            matches = re.findall(pattern, content)
            
            for name, hex_value in matches:
                # Convert hex to decimal
                decimal_value = int(hex_value, 16)
                offsets[name] = decimal_value
                print(f"Found: {name} = {hex_value} ({decimal_value})")
                
        except FileNotFoundError:
            print(f"Error: Could not find {hpp_file_path}")
            return {}
        except Exception as e:
            print(f"Error parsing {hpp_file_path}: {e}")
            return {}
            
        return offsets
    
    def load_existing_json(self, json_file_path: str) -> Dict[str, Any]:
        """Load existing JSON file if it exists"""
        try:
            with open(json_file_path, 'r', encoding='utf-8') as file:
                return json.load(file)
        except FileNotFoundError:
            print(f"JSON file {json_file_path} not found, will create new one")
            return {}
        except json.JSONDecodeError as e:
            print(f"Error parsing JSON file: {e}")
            return {}
    
    def merge_offsets(self, existing_json: Dict[str, Any], hpp_offsets: Dict[str, int]) -> Dict[str, Any]:
        """Merge hpp offsets with existing JSON data"""
        # Start with existing JSON data to preserve any additional fields
        merged = existing_json.copy()
        
        # Update with new offsets from hpp file
        merged.update(hpp_offsets)
        
        return merged
    
    def save_json_file(self, data: Dict[str, Any], json_file_path: str) -> bool:
        """Save the merged data to JSON file"""
        try:
            with open(json_file_path, 'w', encoding='utf-8') as file:
                json.dump(data, file, indent=4, sort_keys=True)
            print(f"Successfully saved {len(data)} offsets to {json_file_path}")
            return True
        except Exception as e:
            print(f"Error saving JSON file: {e}")
            return False
    
    def convert_files(self, hpp_file_path: str = "offsets.hpp", json_file_path: str = "offsets.json"):
        """Main conversion function"""
        print("CS2 Offsets Converter")
        print("=" * 50)
        
        # Parse hpp file
        print(f"Parsing {hpp_file_path}...")
        hpp_offsets = self.parse_hpp_file(hpp_file_path)
        
        if not hpp_offsets:
            print("No offsets found in hpp file!")
            return False
        
        # Load existing JSON
        print(f"\nLoading existing {json_file_path}...")
        existing_json = self.load_existing_json(json_file_path)
        
        # Merge data
        print("\nMerging offsets...")
        merged_data = self.merge_offsets(existing_json, hpp_offsets)
        
        # Save to JSON
        print(f"\nSaving to {json_file_path}...")
        success = self.save_json_file(merged_data, json_file_path)
        
        if success:
            print(f"\nConversion completed successfully!")
            print(f"Total offsets in JSON: {len(merged_data)}")
            
            # Show some statistics
            new_offsets = set(hpp_offsets.keys()) - set(existing_json.keys())
            updated_offsets = set(hpp_offsets.keys()) & set(existing_json.keys())
            
            if new_offsets:
                print(f"New offsets added: {len(new_offsets)}")
                for offset in sorted(new_offsets):
                    print(f"  + {offset}")
            
            if updated_offsets:
                print(f"Existing offsets updated: {len(updated_offsets)}")
                for offset in sorted(updated_offsets):
                    old_val = existing_json.get(offset, "N/A")
                    new_val = hpp_offsets[offset]
                    if old_val != new_val:
                        print(f"  * {offset}: {old_val} → {new_val}")
        
        return success
    
    def validate_conversion(self, json_file_path: str = "offsets.json"):
        """Validate the converted JSON file"""
        print("\nValidating converted file...")
        try:
            with open(json_file_path, 'r') as file:
                data = json.load(file)
            
            print(f"✓ JSON file is valid")
            print(f"✓ Contains {len(data)} entries")
            
            # Check for common CS2 offsets
            common_offsets = ['dwEntityList', 'dwLocalPlayerController', 'dwViewMatrix', 'dwGameRules']
            found_common = [offset for offset in common_offsets if offset in data]
            
            if found_common:
                print(f"✓ Found common CS2 offsets: {', '.join(found_common)}")
            
            return True
            
        except Exception as e:
            print(f"✗ Validation failed: {e}")
            return False

def main():
    """Main function with command line interface"""
    import argparse
    
    parser = argparse.ArgumentParser(description='Convert CS2 offsets from hpp to json')
    parser.add_argument('--hpp', default='offsets.hpp', help='Input hpp file path')
    parser.add_argument('--json', default='offsets.json', help='Output json file path')
    parser.add_argument('--validate', action='store_true', help='Validate the output file')
    
    args = parser.parse_args()
    
    converter = CS2OffsetsConverter()
    
    # Convert files
    success = converter.convert_files(args.hpp, args.json)
    
    # Validate if requested
    if args.validate and success:
        converter.validate_conversion(args.json)

if __name__ == "__main__":
    main()