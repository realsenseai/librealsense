# Custom NN frames, frame layout version 3 (0x0300): host-built

**Host-built, not recorded on a camera.** Each file is the output of the FW's own frame writer, `CustomNnEp13_serialize()` in device-mgr `src/CustomNnEp13.cpp`, compiled on a Linux host (g++ 13.3) at device-mgr `f83ecb08`. Image c6 (7.62.47502.18590) carries the same writer, but c6 has not run on a camera. The tensor bytes are a test pattern, not inference output.

Each file is what the host receives on USB: the frame, then zeros up to 1024 × H. The zero padding follows the USB sink (`UsbHifSinkNode.cpp` `sendCustomNnFrame`, lines 498-512); it was applied by the generator, not by running the sink. The frame is the first 23 + `size` bytes. H = 313 is the height a D585 lists when the DAMO-YOLO sample layout is the stored slot-2 model (320,064-byte frames). Error frames use the same 1024 × 313, as on that camera.

Common values:
- `model_generation` 0x1234ABCD is a test value; on a camera it is the CRC-32 of the slot-2 metadata.
- `timestamp_ms` 123456.789; `source_frame_id` = 136 + `frame_id`.
- Every tensor is s16 (dtype 0). Byte k of output i's data is (i × 37 + k) mod 256.

## Files

| File | Bytes | Frame | Case |
|---|---|---|---|
| `v3_01_ok_model_id_42_1024x313.bin` | 320,512 | 320,064 | OK, `model_id` 42. Six outputs (index 0-5) of 14,912, 384, 59,520, 1,408, 237,952 and 5,440 B; data at 128, 15,104, 15,552, 75,136, 76,608, 314,624 |
| `v3_02_ok_model_id_0_1024x313.bin` | 320,512 | 320,064 | As 01, `model_id` 0: a package with `"model_id": null` gives this |
| `v3_03_ok_model_id_max_1024x313.bin` | 320,512 | 320,064 | As 01, `model_id` 4,294,967,295 |
| `v3_04_ok_small_unaligned_1024x1.bin` | 1,024 | 576 | OK, `model_id` 7. Outputs in DSP order index 2, 0, 1 of 6, 130 and 64 B (data at 128, 256, 512), so the data is padded inside the frame |
| `v3_05_load_failed_gen0_1024x313.bin` | 320,512 | 64 | LOAD_FAILED (4), `model_generation` 0 and `model_id` 0: no model to run (for example a slot-2 install since boot) |
| `v3_06_load_failed_model_1024x313.bin` | 320,512 | 64 | LOAD_FAILED (4) for a stored model whose load failed: generation and `model_id` 42 set |
| `v3_07_dsp_error_1024x313.bin` | 320,512 | 64 | DSP_ERROR (5) |
| `v3_08_no_metadata_1024x313.bin` | 320,512 | 64 | NO_METADATA (1) |
| `v3_09_too_large_1024x313.bin` | 320,512 | 64 | TOO_LARGE (2), made by the writer itself: the six outputs of 01 offered to a 4,096-byte buffer |
| `v3_10_tensor_invalid_1024x313.bin` | 320,512 | 64 | TENSOR_INVALID (3) |
| `v2_90_refused_ok_1024x313.bin` | 320,512 | 320,064 | Version 2 (0x0200, 19-byte header, no `model_id`), the same tensors as 01. Built by the same writer at device-mgr `68e95e53`. A version 3 parser refuses it as an unknown version |
| `v3_91_bad_crc_1024x313.bin` | 320,512 | | 01 with byte 128 XOR 0x01; refused, CRC mismatch |
| `v3_92_bad_size_1024x313.bin` | 320,512 | | 01 with `size` (byte 7) = 320,512, so the frame would end past the wire frame; refused |
| `v3_93_unknown_version_1024x313.bin` | 320,512 | | 01 with `version` (byte 4) = 0x0400; refused |

In every error frame (status 1-5) `flags` = 1, `tensor_count` = 0 and the frame is 64 bytes (`size` 41). In the OK frames `flags` = 0. Files 91-93 were changed by the generator after the writer ran, to test refusals.

sha256 of every file: `SHA256SUMS`.

## Checks

Each file was read by two parsers:
- the struct reading in `generator/make_frames.py`, which checks every field, the CRC (zlib), the zero padding and the tensor pattern;
- hkr-test `scripts/custom_nn_frame.py` `parse_frame(blob, padded_to=len)` at `0c22f1cb`.

Both agree on every field of files 01-10. The parser refuses 90-93 with the reason given above.

## Not covered

- Real inference output and timestamps paired with Color. These need the c6 camera run.
- Version 1 frames (D555E test FW).
- NN_INFO replies.

## Rebuild

`generator/run.sh`: it takes the version 2 writer from device-mgr `68e95e53` with `git show`, compiles `generator/driver.cpp` with the writer, and runs `generator/make_frames.py`. The paths in it are those of the HKR build tree.
