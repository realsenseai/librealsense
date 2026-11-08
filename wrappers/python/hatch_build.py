import os
import shutil
import subprocess
import sys

from hatchling.builders.hooks.plugin.interface import BuildHookInterface

# With infer_tag = True, Hatchling will automatically infer the tag
# based on the current environment (the platform, Python version, etc.)
class CustomBuildHook(BuildHookInterface):
    def initialize(self, version, build_data):
        build_data['infer_tag'] = True
        shutil.rmtree(os.path.join(self.root, 'pyrealsense2', 'pyrealsense2'), ignore_errors=True)
        subprocess.check_call([sys.executable, '-m', 'pybind11_stubgen', 'pyrealsense2.pyrealsense2',
                               '-o', self.root, '--numpy-array-wrap-with-annotated'],
                              env=dict(os.environ, PYTHONPATH=self.root))
