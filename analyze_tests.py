#!/usr/bin/env python3
"""
Test Analysis Script
Extracts test information from all test files in unit-tests/live/
"""

import os
import re
import json
from pathlib import Path
from typing import Dict, List, Tuple

def extract_device_requirements(content: str) -> List[str]:
    """Extract device requirements from test comments"""
    devices = []
    for line in content.split('\n')[:50]:  # Check first 50 lines
        if line.strip().startswith('#test:device'):
            device = line.replace('#test:device', '').strip()
            devices.append(device)
    return devices if devices else ['Not specified']

def extract_test_description(filepath: str, content: str) -> str:
    """Extract test description from docstrings, comments, or test.start() calls"""
    descriptions = []
    
    # Check for docstrings
    docstring_match = re.search(r'"""(.*?)"""', content, re.DOTALL)
    if docstring_match:
        desc = docstring_match.group(1).strip()
        if desc and len(desc) > 10:
            descriptions.append(desc[:500])  # Limit to 500 chars
    
    # Check for test.start() calls
    test_starts = re.findall(r'test\.start\((.*?)\)', content[:5000], re.DOTALL)
    for match in test_starts[:3]:  # First 3 test.start calls
        cleaned = match.strip('"\'').strip()
        if cleaned:
            descriptions.append(cleaned[:200])
    
    # Check for header comments
    lines = content.split('\n')
    comment_block = []
    for i, line in enumerate(lines[5:30], start=5):  # Lines 5-30
        if line.strip().startswith('#') and not line.strip().startswith('#test:'):
            comment = line.strip('#').strip()
            if comment and not comment.startswith('License') and not comment.startswith('Copyright'):
                comment_block.append(comment)
        elif comment_block and not line.strip().startswith('#'):
            break
    
    if comment_block:
        descriptions.append(' '.join(comment_block[:5]))
    
    return ' | '.join(descriptions[:3]) if descriptions else 'No description found'

def extract_kpis(content: str) -> List[str]:
    """Extract KPIs, thresholds, and success criteria"""
    kpis = []
    
    # Common KPI patterns
    patterns = [
        (r'(?:fps|FPS|frame.*rate).*?[=<>]+\s*(\d+(?:\.\d+)?)', 'FPS: {}'),
        (r'(?:threshold|THRESHOLD).*?[=<>]+\s*(\d+(?:\.\d+)?)', 'Threshold: {}'),
        (r'(?:tolerance|TOLERANCE).*?[=<>]+\s*(\d+(?:\.\d+)?)', 'Tolerance: {}'),
        (r'(?:accuracy|ACCURACY).*?[=<>]+\s*(\d+(?:\.\d+)?)', 'Accuracy: {}'),
        (r'(?:delta|DELTA).*?[=<>]+\s*(\d+(?:\.\d+)?)', 'Delta: {}'),
        (r'(?:timeout|TIMEOUT).*?[=<>]+\s*(\d+(?:\.\d+)?)', 'Timeout: {}'),
        (r'(?:duration|DURATION).*?[=<>]+\s*(\d+(?:\.\d+)?)', 'Duration: {}'),
        (r'(?:max|MAX|maximum).*?[=<>]+\s*(\d+(?:\.\d+)?)', 'Max value: {}'),
        (r'(?:min|MIN|minimum).*?[=<>]+\s*(\d+(?:\.\d+)?)', 'Min value: {}'),
    ]
    
    for pattern, format_str in patterns:
        matches = re.findall(pattern, content[:10000], re.IGNORECASE)
        for match in matches[:2]:  # Limit to 2 matches per pattern
            kpis.append(format_str.format(match))
    
    # Look for constant definitions
    const_pattern = r'^([A-Z_]+)\s*=\s*([0-9.]+)'
    for line in content.split('\n')[:200]:
        match = re.match(const_pattern, line.strip())
        if match and any(keyword in match.group(1) for keyword in 
                        ['THRESHOLD', 'TOLERANCE', 'DELTA', 'FPS', 'TIMEOUT', 'MAX', 'MIN', 'DURATION']):
            kpis.append(f'{match.group(1)}: {match.group(2)}')
    
    return kpis if kpis else ['No specific KPIs found']

def extract_test_flags(content: str) -> Dict[str, str]:
    """Extract test execution flags"""
    flags = {}
    for line in content.split('\n')[:50]:
        if '#test:donotrun' in line:
            flags['run_condition'] = line.replace('#test:donotrun', '').strip().strip(':')
        if '#test:timeout' in line:
            timeout = re.search(r'#test:timeout\s+(\d+)', line)
            if timeout:
                flags['timeout'] = f'{timeout.group(1)}s'
        if '#test:retries' in line:
            retries = re.search(r'#test:retries\s+(\d+)', line)
            if retries:
                flags['retries'] = retries.group(1)
        if '#test:priority' in line:
            priority = re.search(r'#test:priority\s+(\d+)', line)
            if priority:
                flags['priority'] = priority.group(1)
    return flags

def analyze_test_file(filepath: Path) -> Dict:
    """Analyze a single test file"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        rel_path = filepath.relative_to(filepath.parents[2])
        category = '/'.join(rel_path.parts[2:-1]) if len(rel_path.parts) > 3 else 'root'
        
        return {
            'filename': filepath.name,
            'path': str(rel_path),
            'category': category,
            'devices': extract_device_requirements(content),
            'description': extract_test_description(str(filepath), content),
            'kpis': extract_kpis(content),
            'flags': extract_test_flags(content),
            'language': 'Python' if filepath.suffix == '.py' else 'C++'
        }
    except Exception as e:
        return {
            'filename': filepath.name,
            'path': str(filepath),
            'category': 'error',
            'error': str(e)
        }

def main():
    """Main analysis function"""
    base_path = Path('/home/tri/GitHub/lrs/librealsense.development/unit-tests/live')
    
    # Find all test files
    test_files = []
    test_files.extend(base_path.rglob('test-*.py'))
    test_files.extend(base_path.rglob('test-*.cpp'))
    test_files = sorted(test_files)
    
    print(f"Found {len(test_files)} test files")
    print("=" * 80)
    
    # Organize by category
    tests_by_category = {}
    
    for filepath in test_files:
        test_info = analyze_test_file(filepath)
        category = test_info['category']
        
        if category not in tests_by_category:
            tests_by_category[category] = []
        
        tests_by_category[category].append(test_info)
    
    # Output results
    output = {
        'summary': {
            'total_tests': len(test_files),
            'categories': len(tests_by_category),
            'python_tests': sum(1 for f in test_files if f.suffix == '.py'),
            'cpp_tests': sum(1 for f in test_files if f.suffix == '.cpp')
        },
        'tests_by_category': tests_by_category
    }
    
    # Save to JSON
    output_file = base_path.parent / 'test_analysis_results.json'
    with open(output_file, 'w') as f:
        json.dump(output, f, indent=2)
    
    print(f"\nAnalysis complete. Results saved to: {output_file}")
    
    # Print summary
    print("\n" + "=" * 80)
    print("SUMMARY BY CATEGORY")
    print("=" * 80)
    
    for category in sorted(tests_by_category.keys()):
        tests = tests_by_category[category]
        print(f"\n{category.upper()} ({len(tests)} tests)")
        print("-" * 80)
        for test in tests:
            print(f"\n  📄 {test['filename']}")
            print(f"     Devices: {', '.join(test.get('devices', ['N/A'])[:3])}")
            desc = test.get('description', 'N/A')
            if len(desc) > 150:
                desc = desc[:147] + '...'
            print(f"     Description: {desc}")
            if test.get('kpis'):
                print(f"     KPIs: {', '.join(test['kpis'][:3])}")
            if test.get('flags'):
                print(f"     Flags: {test['flags']}")

if __name__ == '__main__':
    main()
