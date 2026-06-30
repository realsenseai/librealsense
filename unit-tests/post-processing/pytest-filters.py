# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pyrealsense2 as rs, os, random, csv
import pytest
from pytest_check import check
from rspy import repo
import urllib.request
from urllib.error import URLError, HTTPError
import platform

def download_file(url, subdir, filename):
    destination = os.path.join(subdir, filename)
    try:
        if not os.path.exists(subdir):
            os.makedirs(subdir, exist_ok=True)
            print(f"Created directory: {subdir}")

        if not os.path.exists(destination):
            print(f"Downloading {url}/{filename} to {destination}")
            try:
                req = urllib.request.Request(url)
                with urllib.request.urlopen(req) as response, open(destination, 'wb') as out_file:
                    out_file.write(response.read())
                print(f"Downloaded {filename} successfully.")
            except (URLError, HTTPError) as e:
                print(f"Failed to download {url}/{filename}: {e.reason}")
                return False
    except OSError as e:
        print(f"Failed to create directory {subdir}: {e.strerror}")
        return False

    return True

RECORDINGS_FOLDER = os.path.join(repo.build, 'unit-tests', 'recordings')

if platform.system() == 'Windows':
    Deployment_Location = os.getenv('TESTDATA_LOCATION', os.getenv('TEMP', 'C:\\Temp'))
else:
    Deployment_Location = os.getenv('TESTDATA_LOCATION', '/tmp/')

PP_Tests_List = [
    "1551257764229", "1551257812956", "1551257880762", "1551257882796",
    "1551257884097", "1551257987255", "1551259481873", "1551261946511",
    "1551262153516", "1551262256875", "1551262841203", "1551262772964",
    "1551262971309", "1551263177558"
]

# Extensions for post-processing test files
PP_Test_extensions_List = [".Input.raw", ".Input.csv", ".Output.raw", ".Output.csv"]

PP_TESTS_URL = "https://librealsense.realsenseai.com/rs-tests/post_processing_tests_2018_ww18/"

def get_sequence_length(contents):
    sequence_length = 1
    for line in contents.split('\n'):
        if "Frames sequence length" in line:
            parts = line.split(',')
            if len(parts) > 1 and parts[1].isdigit():
                sequence_length = int(parts[1])
                break
    return sequence_length

def produce_sequence_extensions(source, target):
    if not os.path.exists(target):
        download_file(PP_TESTS_URL + source, RECORDINGS_FOLDER, source)
    with open(target, 'r') as f:
        contents = f.read()
    sequence_length = get_sequence_length(contents)
    return [f".{i}" for i in range(sequence_length)]


@pytest.fixture(scope="module", autouse=True)
def download_dataset():
    print(f"Preparing to download Post-processing tests dataset...\nRemote server: {PP_TESTS_URL}\nTarget Location: {Deployment_Location}")

    for test_pattern in PP_Tests_List:
        sequence_meta_file = f"{test_pattern}.0.Output.csv"
        source = sequence_meta_file
        destination = os.path.join(RECORDINGS_FOLDER, sequence_meta_file)
        sequence_extensions = produce_sequence_extensions(source, destination)

        for ext in PP_Test_extensions_List:
            for idx in sequence_extensions:
                test_file_name = f"{test_pattern}{idx}{ext}"
                source = test_file_name
                destination = os.path.join(RECORDINGS_FOLDER, test_file_name)
                download_file(PP_TESTS_URL + test_file_name, RECORDINGS_FOLDER, test_file_name)

    for test_pattern in PP_Tests_List:
        sequence_meta_file = f"{test_pattern}.0.Output.csv"
        source = sequence_meta_file
        destination = os.path.join(RECORDINGS_FOLDER, sequence_meta_file)
        sequence_extensions = produce_sequence_extensions(source, destination)

        for ext in PP_Test_extensions_List:
            for idx in sequence_extensions:
                test_file_name = f"{test_pattern}{idx}{ext}"
                source = test_file_name
                destination = os.path.join(RECORDINGS_FOLDER, test_file_name)
                download_file(PP_TESTS_URL + test_file_name, RECORDINGS_FOLDER, test_file_name)

################################################################################################
frame_metadata_count = 43

ppf_test_cases = [
    # All the tests below involve transforming between the depth and disparity domains.
    # We only test downsample scales 2 and 3 because scales 4-7 are implemented differently from the reference code:
    # In Librealsense, the filter uses the mean of depth for all scales [2-8].
    # In the reference code, the filter uses the mean of depth for scales [2-3], but switches to the mean of disparities for scales [4-7] due to implementation constraints.
    ("1551257764229", "D435_DS(2)"),
    ("1551257812956", "D435_DS(3)"),
    # Downsample + Hole-Filling modes 0/1/2
    ( "1551257880762","D435_DS(2)_HoleFill(0)" ),
    ( "1551257882796","D435_DS(2)_HoleFill(1)" ),
    ( "1551257884097","D435_DS(2)_HoleFill(2)" ),
    # Downsample + Spatial Filter parameters
    ( "1551257987255",  "D435_DS(2)+Spat(A:0.85/D:32/I:3)" ),
    ( "1551259481873",  "D435_DS(2)+Spat(A:0.5/D:15/I:2)" ),
    #Downsample + Temporal Filter
    ( "1551261946511",  "D435_DS(2)+Temp(A:0.25/D:15/P:0)" ),
    ( "1551262153516",  "D435_DS(2)+Temp(A:0.45/D:25/P:1)" ),
    ( "1551262256875",  "D435_DS(2)+Temp(A:0.5/D:30/P:4)" ),
    ( "1551262841203",  "D435_DS(2)+Temp(A:0.5/D:30/P:6)" ),
    ( "1551262772964",  "D435_DS(2)+Temp(A:0.5/D:30/P:8)" ),
    # Downsample + Spatial + Temporal
    ( "1551262971309",  "D435_DS(2)_Spat(A:0.7/D:25/I:2)_Temp(A:0.6/D:15/P:6)" ),
    # Downsample + Spatial + Temporal (+ Hole-Filling)
    ( "1551263177558",  "D435_DS(2)_Spat(A:0.7/D:25/I:2)_Temp(A:0.6/D:15/P:6))_HoleFill(1)" ),
]

class post_proccesing_filters:
    def __init__(self):
        self.depth_to_disparity = rs.disparity_transform(True)
        self.disparity_to_depth = rs.disparity_transform(False)
        self.use_decimation = False
        self.use_spatial = False
        self.use_temporal = False
        self.use_holes = False
        self.decimation_filter = rs.decimation_filter()
        self.spatial_filter = rs.spatial_filter()
        self.temporal_filter = rs.temporal_filter()
        self.hole_filling_filter = rs.hole_filling_filter()

    def configure(self,filters_cfg):
        #Reconfigure the post-processing according to the test spec
        self.use_decimation = filters_cfg.downsample_scale != 1
        self.decimation_filter.set_option(rs.option.filter_magnitude, filters_cfg.downsample_scale)

        self.use_spatial = filters_cfg.spatial_filter
        self.use_temporal = filters_cfg.temporal_filter
        self.use_holes = filters_cfg.holes_filter

        if filters_cfg.spatial_filter:
            self.spatial_filter.set_option(rs.option.filter_smooth_alpha, filters_cfg.spatial_alpha)
            self.spatial_filter.set_option(rs.option.filter_smooth_delta, filters_cfg.spatial_delta)
            self.spatial_filter.set_option(rs.option.filter_magnitude,filters_cfg.spatial_iterations)

        if filters_cfg.temporal_filter:
            self.temporal_filter.set_option(rs.option.filter_smooth_alpha, filters_cfg.temporal_alpha)
            self.temporal_filter.set_option(rs.option.filter_smooth_delta, filters_cfg.temporal_delta)
            self.temporal_filter.set_option(rs.option.holes_fill, filters_cfg.temporal_persistence)

        if filters_cfg.holes_filter:
            self.hole_filling_filter.set_option(rs.option.holes_fill, filters_cfg.holes_filling_mode)

    def process(self, frame_input):
        processed = frame_input
        if self.use_decimation:
            processed = self.decimation_filter.process(processed)

        processed = self.depth_to_disparity.process(processed)

        if self.use_spatial:
            processed = self.spatial_filter.process(processed)

        if self.use_temporal:
            processed = self.temporal_filter.process(processed)

        processed = self.disparity_to_depth.process(processed)

        if self.use_holes:
            processed = self.hole_filling_filter.process(processed)

        return processed

class ppf_test_config:
    def __init__(self):
        self.name = ""
        self.spatial_filter = False
        self.spatial_alpha = 0.0
        self.spatial_delta = 0
        self.spatial_iterations = None
        self.temporal_filter = False
        self.temporal_alpha = 0.0
        self.temporal_delta = 0
        self.temporal_persistence = 0
        self.holes_filter = False
        self.holes_filling_mode = 0
        self.downsample_scale = 1
        self.depth_units = 0.001
        self.stereo_baseline_mm = 0.0
        self.focal_length = 0.0
        self.input_res_x = 0
        self.input_res_y = 0
        self.output_res_x = 0
        self.output_res_y = 0
        self.frames_sequence_size = 1
        self.input_frame_names = []
        self.output_frame_names =[]
        self._input_frames = []
        self._output_frames = []

    def reset(self):
        self.name = ""
        self.spatial_filter = self.temporal_filter = self.holes_filter = False
        self.spatial_alpha = self.temporal_alpha = 0
        self.spatial_iterations = self.temporal_persistence = self.holes_filling_mode = self.spatial_delta = self.temporal_delta = 0
        self.input_res_x = self.input_res_y = self.output_res_x = self.output_res_y = 0
        self.downsample_scale = 1
        self._input_frames = []
        self._output_frames = []


def load_from_binary(file_name):
    with open(file_name, 'rb') as binary_file:
        loaded_data = binary_file.read()
    return loaded_data


def attrib_from_csv(file_path):

    with open(file_path, 'r') as csv_file:
        reader = csv.reader(csv_file)
        dict = {row[0]: row[1] for row in reader}

    cfg = ppf_test_config()

    cfg.input_res_x = int(dict.get('Resolution_x', 0))
    cfg.input_res_y = int(dict.get('Resolution_y', 0))
    cfg.stereo_baseline_mm = float(dict.get('Stereo Baseline', 0.0))
    cfg.depth_units = float(dict.get('Depth Units', 0.0))
    cfg.focal_length = float(dict.get('Focal Length', 0.0))

    cfg.downsample_scale = int(dict.get('Scale', 1))
    cfg.spatial_filter = 'Spatial Filter Params:' in dict
    cfg.spatial_alpha = float(dict.get('SpatialAlpha', 0.0))
    cfg.spatial_delta = float(dict.get('SpatialDelta', 0))
    cfg.spatial_iterations = int(dict.get('SpatialIterations', 0))
    cfg.temporal_filter = 'Temporal Filter Params:' in dict
    cfg.temporal_alpha = float(dict.get('TemporalAlpha', 0.0))
    cfg.temporal_delta = float(dict.get('TemporalDelta', 0))
    cfg.temporal_persistence = int(dict.get('TemporalPersistency', 0))

    cfg.holes_filter = 'Holes Filling Mode:' in dict
    cfg.holes_filling_mode = int(dict.get('HolesFilling', 0))

    cfg.frames_sequence_size = int(dict.get('Frames sequence length', 0))

    cfg.input_frame_names = [dict.get(str(i), '') + ".raw" for i in range(cfg.frames_sequence_size)]

    return cfg


def check_load_test_configuration(test_config):
    check.is_true(test_config.name)
    check.is_true(test_config.input_res_x)
    check.is_true(test_config.input_res_y)

    for i in range(test_config.frames_sequence_size):
        check.is_true(len(test_config._input_frames[i]))
        check.is_true(len(test_config._output_frames[i]))

    _padded_width = int((test_config.input_res_x // test_config.downsample_scale) + 3) // 4 * 4
    _padded_height = int((test_config.input_res_y // test_config.downsample_scale) + 3) // 4 * 4

    check.equal(test_config.output_res_x, _padded_width)
    check.equal(test_config.output_res_y, _padded_height)
    check.is_true(test_config.input_res_x > 0)
    check.is_true(test_config.input_res_y > 0)
    check.is_true(test_config.output_res_x > 0)
    check.is_true(test_config.output_res_y > 0)
    check.is_true(abs(test_config.stereo_baseline_mm) > 0)
    check.is_true(test_config.depth_units > 0)
    check.is_true(test_config.focal_length > 0)
    check.is_true(test_config.frames_sequence_size > 0)

    for i in range(test_config.frames_sequence_size):
        check.equal(test_config.input_res_x * test_config.input_res_y * 2, len(test_config._input_frames[i]))
        check.equal(test_config.output_res_x * test_config.output_res_y * 2,len(test_config._output_frames[i]))

    # The following tests use assumption about the filters intrinsic
    # Note that the specific parameter threshold are verified as of April 2018
    if test_config.spatial_filter:
        check.is_true(test_config.spatial_alpha >= 0.25)
        check.is_true(test_config.spatial_alpha <= 1.0)
        check.is_true(test_config.spatial_delta >= 1)
        check.is_true(test_config.spatial_delta <= 50)
        check.is_true(test_config.spatial_iterations >= 1)
        check.is_true(test_config.spatial_iterations <= 5)

    if test_config.temporal_filter:
        check.is_true(test_config.temporal_alpha >= 0)
        check.is_true(test_config.temporal_alpha <= 1.0)
        check.is_true(test_config.temporal_delta >= 1)
        check.is_true(test_config.temporal_delta <= 100)
        check.is_true(test_config.temporal_persistence >= 0)

    if test_config.holes_filter:
        check.is_true(test_config.holes_filling_mode >= 0)
        check.is_true(test_config.holes_filling_mode <= 2)

    return True

def load_test_configuration(test_name, test_config):
    base_name = os.path.join(repo.build, 'unit-tests', 'recordings', test_name)
    base_name += ".0"
    test_file_names = {
        "Input_pixels": ".Input.raw",
        "Input_metadata": ".Input.csv",
        "Output_pixels": ".Output.raw",
        "Output_metadata": ".Output.csv",
    }

    #checking all the files are present
    for test_name in test_file_names.values():
        assert os.path.exists(base_name+test_name)

    #Prepare the configuration data set
    test_config.reset()
    test_config.name = test_name
    input_meta_params = attrib_from_csv(base_name + test_file_names["Input_metadata"])
    output_meta_params = attrib_from_csv(base_name + test_file_names["Output_metadata"])
    test_config.frames_sequence_size = input_meta_params.frames_sequence_size
    test_config.input_frame_names = input_meta_params.input_frame_names
    test_config.output_frame_names = output_meta_params.input_frame_names

    for i in range(test_config.frames_sequence_size):
        test_config._input_frames.append(load_from_binary(os.path.join(repo.build, 'unit-tests', 'recordings', test_config.input_frame_names[i])))
        test_config._output_frames.append(load_from_binary(os.path.join(repo.build, 'unit-tests', 'recordings', test_config.output_frame_names[i])))

    test_config.input_res_x = input_meta_params.input_res_x
    test_config.input_res_y = input_meta_params.input_res_y
    test_config.output_res_x = output_meta_params.input_res_x
    test_config.output_res_y = output_meta_params.input_res_y
    test_config.depth_units = input_meta_params.depth_units
    test_config.focal_length = input_meta_params.focal_length
    test_config.stereo_baseline_mm = input_meta_params.stereo_baseline_mm
    test_config.downsample_scale = output_meta_params.downsample_scale
    test_config.spatial_filter = output_meta_params.spatial_filter
    test_config.spatial_alpha = output_meta_params.spatial_alpha
    test_config.spatial_delta = output_meta_params.spatial_delta
    test_config.spatial_iterations = output_meta_params.spatial_iterations
    test_config.holes_filter = output_meta_params.holes_filter
    test_config.holes_filling_mode = output_meta_params.holes_filling_mode
    test_config.temporal_filter = output_meta_params.temporal_filter
    test_config.temporal_alpha = output_meta_params.temporal_alpha
    test_config.temporal_delta = output_meta_params.temporal_delta
    test_config.temporal_persistence = output_meta_params.temporal_persistence

    return check_load_test_configuration(test_config)

def profile_diffs(distances, max_allowed_std, outlier):
    if distances:
        mean = sum(distances) / len(distances)
    else:
        return

    max_value = max(distances)
    check.equal(max_value, 0)

    e = 0
    inverse = 1/len(distances)
    for distance in distances:
        e += (distance - mean) ** 2
    standard_deviation = (inverse * e) ** 0.5

    check.is_true(standard_deviation <= max_allowed_std)
    check.is_true(abs(max_value) <= outlier)

    return (abs(max_value) <= outlier) and (standard_deviation <= max_allowed_std)

def validate_ppf_results(result_depth, reference_data, frame_idx):
    diff2orig = []

    domain_transform_only = (reference_data.downsample_scale == 1) and (not reference_data.spatial_filter) and (not reference_data.temporal_filter)
    result_profile = result_depth.get_profile().as_video_stream_profile()
    check.equal(result_profile.width(), reference_data.output_res_x)
    check.equal(result_profile.height(), reference_data.output_res_y)

    #Pixel-by-pixel comparison of the resulted filtered depth vs data encoded with external tool
    v1 = bytearray(result_depth.get_data())
    v2 = (reference_data._output_frames[frame_idx])
    check.equal(len(v1), len(v2))
    diff2ref = [abs(byte1 - byte2) for byte1, byte2 in zip(v1, v2)]

    #Validate the filters
    #The differences between the reference code and librealsense implementation are byte-compared below
    return profile_diffs(diff2ref, 0, 0)

def compare_frame_md(origin_depth, result_depth):
    for i in range(frame_metadata_count):
        origin_supported = origin_depth.supports_frame_metadata(rs.frame_metadata_value(i))
        result_supported = result_depth.supports_frame_metadata(rs.frame_metadata_value(i))
        check.equal(origin_supported, result_supported)
        if origin_supported and result_supported:
            #FRAME_TIMESTAMP and SENSOR_TIMESTAMP metadatas are not included in post proccesing frames,
            #TIME_OF_ARRIVAL continues to increase  after post proccesing
            if i == rs.frame_metadata_value.frame_timestamp or i == rs.frame_metadata_value.sensor_timestamp or i == rs.frame_metadata_value.time_of_arrival:
                continue

            origin_val = origin_depth.get_frame_metadata(rs.frame_metadata_value(i))
            result_val = result_depth.get_frame_metadata(rs.frame_metadata_value(i))
            check.equal(origin_val, result_val)

def create_depth_intrinsics(test_cfg):
    depth_intrinsics = rs.intrinsics()
    depth_intrinsics.width = test_cfg.input_res_x
    depth_intrinsics.height = test_cfg.input_res_y
    depth_intrinsics.ppx = test_cfg.input_res_x / 2.0
    depth_intrinsics.ppy = test_cfg.input_res_y / 2.0
    depth_intrinsics.fx = test_cfg.focal_length
    depth_intrinsics.fy = test_cfg.focal_length
    depth_intrinsics.model = rs.distortion.brown_conrady
    depth_intrinsics.coeffs = [0, 0, 0, 0, 0]
    return depth_intrinsics

def create_video_stream(test_cfg, depth_intrinsics):
    vs = rs.video_stream()
    vs.type = rs.stream.depth
    vs.index = 0
    vs.uid = 0
    vs.width = test_cfg.input_res_x
    vs.height = test_cfg.input_res_y
    vs.fps = 30
    vs.bpp = 2
    vs.fmt = rs.format.z16
    vs.intrinsics = depth_intrinsics
    return vs

def create_frame(test_cfg, depth_stream_profile, index):
    frame = rs.software_video_frame()
    frame.pixels = test_cfg._input_frames[index]
    frame.bpp = 2
    frame.stride = test_cfg.input_res_x * 2
    frame.timestamp = index + 1
    frame.domain = rs.timestamp_domain.system_time
    frame.frame_number = index
    frame.profile = depth_stream_profile.as_video_stream_profile()
    frame.depth_units = test_cfg.depth_units
    return frame


################################################################################################
def test_filters_sequence_validation():
    test_cfg = ppf_test_config()
    for first_element, second_element in ppf_test_cases:
        # Load the data from configuration and raw frame files
        if not load_test_configuration(first_element, test_cfg):
            continue

        ppf = post_proccesing_filters()
        ppf.configure(test_cfg)

        sw_dev = rs.software_device()
        depth_sensor = sw_dev.add_sensor("Depth")

        depth_intrinsics = create_depth_intrinsics(test_cfg)

        vs = create_video_stream(test_cfg, depth_intrinsics)
        depth_stream_profile = depth_sensor.add_video_stream(vs)
        depth_sensor.add_read_only_option(rs.option.depth_units, test_cfg.depth_units)
        depth_sensor.add_read_only_option(rs.option.stereo_baseline, test_cfg.stereo_baseline_mm)

        sw_dev.create_matcher(rs.matchers.dlr_c)
        sync = rs.syncer()

        depth_sensor.open(depth_stream_profile)
        depth_sensor.start(sync)

        frames = test_cfg.frames_sequence_size if test_cfg.frames_sequence_size > 1 else 1
        for i in range(frames):
            frame = create_frame(test_cfg, depth_stream_profile, i)
            depth_sensor.on_video_frame(frame)

            fset = sync.wait_for_frames()
            depth = fset.first_or_default(rs.stream.depth)

            #the actual filters are being applied
            filtered_depth = ppf.process(depth)
            validate_ppf_results(filtered_depth, test_cfg, i)

        depth_sensor.stop()
        depth_sensor.close()


################################################################################################
def test_filters_metadata_validation():
    test_cfg = ppf_test_config()
    for first_element, second_element in ppf_test_cases:
        # Load the data from configuration and raw frame files
        if not load_test_configuration(first_element, test_cfg):
            continue

        ppf = post_proccesing_filters()

        sw_dev = rs.software_device()
        depth_sensor = sw_dev.add_sensor("Depth")

        depth_intrinsics = create_depth_intrinsics(test_cfg)

        vs = create_video_stream(test_cfg, depth_intrinsics)
        depth_stream_profile = depth_sensor.add_video_stream(vs)
        depth_sensor.add_read_only_option(rs.option.depth_units, test_cfg.depth_units)
        depth_sensor.add_read_only_option(rs.option.stereo_baseline, test_cfg.stereo_baseline_mm)

        sw_dev.create_matcher(rs.matchers.dlr_c)
        sync = rs.syncer()

        depth_sensor.open(depth_stream_profile)
        depth_sensor.start(sync)

        frames = test_cfg.frames_sequence_size if test_cfg.frames_sequence_size > 1 else 1
        for i in range(frames):
            for j in range(frame_metadata_count):
                depth_sensor.set_metadata(rs.frame_metadata_value(i), random.randint(0,99))

            frame = create_frame(test_cfg, depth_stream_profile, i)
            depth_sensor.on_video_frame(frame)

            fset = sync.wait_for_frames()
            depth = fset.first_or_default(rs.stream.depth)

            # the actual filters are being applied
            filtered_depth = ppf.process(depth)

            #Compare the resulted frame metadata versus input
            compare_frame_md(depth, filtered_depth)

        depth_sensor.stop()
        depth_sensor.close()
