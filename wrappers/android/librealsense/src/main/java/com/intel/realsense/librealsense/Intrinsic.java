package com.intel.realsense.librealsense;

public class Intrinsic {
    private int mWidth;
    private int mHeight;
    private float mPpx;
    private float mPpy;
    private float mFx;
    private float mFy;
    private DistortionType mModel;
    private int mModelValue;
    private float[] mCoeffs;


    public Intrinsic(){
        mCoeffs = new float[5];
    }

    public Intrinsic(int width, int height,
                     float ppx, float ppy,
                     float fx, float fy,
                     int model,
                     float[] coeffs){
        this.mWidth = width;
        this.mHeight = height;
        this.mPpx = ppx;
        this.mPpy = ppy;
        this.mFx = fx;
        this.mFy = fy;
        this.mModel = DistortionType.values()[model];
        this.mModelValue = model;
        this.mCoeffs = coeffs;
    }

    public int getWidth() {return mWidth;}
    public int getHeight() {return mHeight;}
    public float getPpx() {return mPpx;}
    public float getPpy() {return mPpy;}
    public float getFx() {return mFx;}
    public float getFy() {return mFy;}
    public DistortionType getModel() {return mModel;}
    public float[] getCoeffs() {return mCoeffs;}

    public void SetModel(){
        this.mModel = DistortionType.values()[mModelValue];
    }
}
