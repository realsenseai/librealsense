package com.intel.realsense.camera;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.Toast;

import com.intel.realsense.librealsense.Device;
import com.intel.realsense.librealsense.DeviceList;
import com.intel.realsense.librealsense.Extension;
import com.intel.realsense.librealsense.ProgressListener;
import com.intel.realsense.librealsense.RsContext;
import com.intel.realsense.librealsense.Updatable;
import com.intel.realsense.librealsense.UpdateDevice;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

public class FirmwareUpdateProgressDialog extends DialogFragment {
    private static final String TAG = "librs fwupdate";

    private ProgressBar mProgressBar;
    private String mFirmwareFilePath;
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        final Activity activity = getActivity();

        Bundle bundle = getArguments();
        mFirmwareFilePath = bundle == null ? "" : bundle.getString(getString(R.string.firmware_update_file_path), "");

        LayoutInflater inflater = activity.getLayoutInflater();
        View fragmentView = inflater.inflate(R.layout.firmware_update_progress, null);

        mProgressBar = fragmentView.findViewById(R.id.firmwareUpdateProgressBar);

        Thread t = new Thread(mFirmwareUpdate);
        t.start();

        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
        builder.setView(fragmentView);
        AlertDialog rv = builder.create();
        rv.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        return rv;
    }

    private static byte[] readFwFile(InputStream in) throws IOException {
        int length = in.available();
        byte[] rv = new byte[length];
        int len = in.read(rv,0, rv.length);
        in.close();
        return rv;
    }

    private static InputStream getInputStream(String path) throws IOException {
        File fwFile = new File(path);
        return new FileInputStream(fwFile);
    }

    private Runnable mFirmwareUpdate = new Runnable() {
        @Override
        public void run() {
            boolean done = false;
            RsContext ctx = new RsContext();
            boolean notify = false;
            try(DeviceList dl = ctx.queryDevices()){
                if(dl.getDeviceCount() == 0)
                    return;
                try(Device d = dl.createDevice(0)){
                    if(mFirmwareFilePath.equals("")){
                        throw new RuntimeException("Firmware update requires a .bin file. Download one from https://dev.realsenseai.com/docs/firmware-updates");
                    }
                    final byte[] bytes = readFwFile(getInputStream(mFirmwareFilePath));
                    if(d.is(Extension.UPDATE_DEVICE)) {
                        UpdateDevice fwud = d.as(Extension.UPDATE_DEVICE);
                        updateFirmware(fwud, bytes);
                        done = true;
                        notify = true;
                    } else if(d.is(Extension.UPDATABLE)){
                        Updatable fwud = d.as(Extension.UPDATABLE);
                        updateFirmware(fwud, bytes);//TODO: make it work
                    }
                }
            }catch (Exception e) {
                final String msg = "firmware update failed, error: " + e.getMessage();
                Log.e(TAG, msg);

                final Activity activity = getActivity();

                activity.runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            Toast.makeText(activity, msg, Toast.LENGTH_LONG).show();
                        }
                    });
            }finally {
                if(notify)
                    ((DetachedActivity)getActivity()).onFwUpdateStatus(done);
                dismissAllowingStateLoss();
            }
        }
    };

    private void updateFirmware(UpdateDevice device, final byte[] bytes) {
        device.update(bytes, new ProgressListener() {
            @Override
            public void onProgress(final float progress) {
                getActivity().runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        int perc = (int) (progress * 100);
                        mProgressBar.setProgress(perc);
                    }
                });
            }
        });
        Log.i(TAG, "Firmware update process finished successfully");
    }

    private void updateFirmware(Updatable device, final byte[] bytes) {
        device.updateUnsigned(bytes, new ProgressListener() {
            @Override
            public void onProgress(final float progress) {
                getActivity().runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        int perc = (int) (progress * 100);
                        mProgressBar.setProgress(perc);
                    }
                });
            }
        });
        Log.i(TAG, "Firmware update process finished successfully");
    }
}
