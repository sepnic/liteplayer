/*
 * Copyright (C) 2019-2023 Qinglong<sysu.zqlong@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.example.liteplayerdemo;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import androidx.core.app.ActivityCompat;

import com.sepnic.liteplayer.Liteplayer;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class MainActivity extends Activity {
    private final static String TAG = "LiteplayerDemo";
    private Liteplayer mLiteplayer;
    private TextView mStatusView;
    private int mStatus;
    private static final int PERMISSIONS_REQUEST_CODE_EXTERNAL_STORAGE = 1000;

    private void requestPermissions(Activity activity) {
        // request external storage permissions
        if (ActivityCompat.checkSelfPermission(activity, Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            String[] PERMISSION_EXTERNAL_STORAGE =
                    new String[]{ Manifest.permission.READ_EXTERNAL_STORAGE };
            ActivityCompat.requestPermissions(activity, PERMISSION_EXTERNAL_STORAGE, PERMISSIONS_REQUEST_CODE_EXTERNAL_STORAGE);
        }
    }

    @Override
    public void onRequestPermissionsResult(int permsRequestCode, String[] permissions, int[] grantResults) {
        switch (permsRequestCode) {
            case PERMISSIONS_REQUEST_CODE_EXTERNAL_STORAGE:
                for (int result : grantResults) {
                    if (result != PackageManager.PERMISSION_GRANTED) {
                        Toast.makeText(this, "Failed request EXTERNAL_STORAGE permission", Toast.LENGTH_LONG).show();
                        break;
                    }
                }
                break;
            default:
                break;
        }
    }

    private String prepareAssetMusicFile(String assetFile) {
        try {
            File cacheDir = getCacheDir();
            if (!cacheDir.exists()) {
                boolean res = cacheDir.mkdirs();
                if (!res) {
                    return null;
                }
            }
            File outFile = new File(cacheDir, assetFile);
            if (!outFile.exists()) {
                boolean res = outFile.createNewFile();
                if (!res) {
                    return null;
                }
            } else {
                if (outFile.length() > 0) {
                    return outFile.getAbsolutePath();
                }
            }
            InputStream is = getAssets().open(assetFile);
            FileOutputStream fos = new FileOutputStream(outFile);
            byte[] buffer = new byte[1024];
            int byteRead = 0;
            while ((byteRead = is.read(buffer)) > 0) {
                fos.write(buffer, 0, byteRead);
            }
            fos.flush();
            is.close();
            fos.close();
            return outFile.getAbsolutePath();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return null;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        requestPermissions(this);

        mStatusView = findViewById(R.id.statusView);
        mStatusView.setText("Idle");
        mStatus = Liteplayer.LITEPLAYER_IDLE;

        mLiteplayer = new Liteplayer();
        mLiteplayer.setOnIdleListener(mIdleListener);
        mLiteplayer.setOnPreparedListener(mPreparedListener);
        mLiteplayer.setOnStartedListener(mStartedListener);
        mLiteplayer.setOnPausedListener(mPausedListener);
        mLiteplayer.setOnSeekCompletedListener(mSeekCompletedListener);
        mLiteplayer.setOnCompletedListener(mCompletedListener);
        mLiteplayer.setOnErrorListener(mErrorListener);
    }

    @Override
    protected void onDestroy() {
        mLiteplayer.release();
        super.onDestroy();
    }

    public void onStartClick(View view) {
        if (mStatus == Liteplayer.LITEPLAYER_IDLE) {
            //String music = Environment.getExternalStorageDirectory().getAbsolutePath() + "/test.mp3";
            //String music = "https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3";
            String music = prepareAssetMusicFile("test.mp3");
            mLiteplayer.setDataSource(music);
            mLiteplayer.prepareAsync();
        } else if (mStatus == Liteplayer.LITEPLAYER_PAUSED || mStatus == Liteplayer.LITEPLAYER_SEEKCOMPLETED) {
            mLiteplayer.resume();
        }
    }

    public void onPauseClick(View view) {
        if (mStatus == Liteplayer.LITEPLAYER_STARTED) {
            mLiteplayer.pause();
        }
    }

    public void onSeekClick(View view) {
        int position = mLiteplayer.getCurrentPosition();
        int duration = mLiteplayer.getDuration();
        int seekOffset = position + 10*1000;
        if (seekOffset >= duration) {
            Log.e(TAG, "Failed to seek, postion: " + position + "ms, duration: " + duration + "ms");
            return;
        }
        mLiteplayer.seekTo(seekOffset);
    }

    public void onStopClick(View view) {
        if (mStatus != Liteplayer.LITEPLAYER_IDLE) {
            mLiteplayer.reset();
        }
    }

    private final Liteplayer.OnIdleListener mIdleListener = new Liteplayer.OnIdleListener() {
        public void onIdle(Liteplayer p) {
            mStatus = Liteplayer.LITEPLAYER_IDLE;
            mStatusView.setText("Idle");
        }
    };

    private final Liteplayer.OnPreparedListener mPreparedListener = new Liteplayer.OnPreparedListener() {
        public void onPrepared(Liteplayer p) {
            mStatus = Liteplayer.LITEPLAYER_PREPARED;
            mStatusView.setText("Prepared");
            p.start();
        }
    };

    private final Liteplayer.OnStartedListener mStartedListener = new Liteplayer.OnStartedListener() {
        public void onStarted(Liteplayer p) {
            mStatus = Liteplayer.LITEPLAYER_STARTED;
            mStatusView.setText("Started");
        }
    };

    private final Liteplayer.OnPausedListener mPausedListener = new Liteplayer.OnPausedListener() {
        public void onPaused(Liteplayer p) {
            mStatus = Liteplayer.LITEPLAYER_PAUSED;
            mStatusView.setText("Paused");
        }
    };

    private final Liteplayer.OnSeekCompletedListener mSeekCompletedListener = new Liteplayer.OnSeekCompletedListener() {
        public void onSeekCompleted(Liteplayer p) {
            mStatusView.setText("SeekCompleted");
            if (mStatus != Liteplayer.LITEPLAYER_PAUSED) {
                p.start();
            }
        }
    };

    private final Liteplayer.OnCompletedListener mCompletedListener = new Liteplayer.OnCompletedListener() {
        public void onCompleted(Liteplayer p) {
            mStatus = Liteplayer.LITEPLAYER_COMPLETED;
            mStatusView.setText("Completed");
            mLiteplayer.reset();
        }
    };

    private final Liteplayer.OnErrorListener mErrorListener = new Liteplayer.OnErrorListener() {
        public void onError(Liteplayer p, int what, int extra) {
            mStatus = Liteplayer.LITEPLAYER_ERROR;
            mStatusView.setText("Error");
            mLiteplayer.reset();
        }
    };
}
