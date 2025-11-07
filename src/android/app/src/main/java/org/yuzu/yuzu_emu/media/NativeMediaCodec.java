// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.media;

import android.media.Image;
import android.media.ImageReader;
import android.view.Surface;
import android.hardware.HardwareBuffer;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Build;
import android.util.Log;

import java.nio.ByteBuffer;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

public class NativeMediaCodec {
    private static final String TAG = "NativeMediaCodec";
    private static final ConcurrentHashMap<Integer, MediaCodec> decoders = new ConcurrentHashMap<>();
    private static final AtomicInteger nextId = new AtomicInteger(1);

    // Called from native code to create a decoder for the given mime (e.g. "video/avc").
    // Returns a decoder id (>0) on success, or 0 on failure.
    public static int createDecoder(String mime, int width, int height) {
        try {
            MediaCodec codec = MediaCodec.createDecoderByType(mime);
            MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                        MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible);
            }
            final int id = nextId.getAndIncrement();
            decoders.put(id, codec);
            // Use ImageReader-backed Surface so the codec can produce AHardwareBuffer-backed images
            ImageReader imageReader = null;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                imageReader = ImageReader.newInstance(width, height, android.graphics.ImageFormat.YUV_420_888, 4);
                final ImageReader ir = imageReader;
                ir.setOnImageAvailableListener(new ImageReader.OnImageAvailableListener() {
                    @Override
                    public void onImageAvailable(ImageReader reader) {
                        Image image = null;
                        try {
                            image = reader.acquireLatestImage();
                            if (image == null) return;
                            long pts = image.getTimestamp();
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                                HardwareBuffer hb = image.getHardwareBuffer();
                                if (hb != null) {
                                    onHardwareBufferAvailable(id, hb, pts);
                                }
                            }
                        } catch (Throwable t) {
                            Log.w(TAG, "onImageAvailable failed: " + t);
                        } finally {
                            if (image != null) image.close();
                        }
                    }
                }, null);
                codec.setCallback(new MediaCodec.Callback() {
                    private final int decoderId = id;

                    @Override
                    public void onInputBufferAvailable(MediaCodec mc, int index) {
                        // input will be fed by native code via dequeue
                    }

                    @Override
                    public void onOutputBufferAvailable(MediaCodec mc, int index, MediaCodec.BufferInfo info) {
                        // We configure the codec to output to the ImageReader surface; the ImageReader listener
                        // will handle delivering AHardwareBuffer-backed images to native code. Still release the
                        // output buffer if requested by codec (some codecs may not use getOutputImage when a
                        // Surface is provided).
                        try {
                            try { mc.releaseOutputBuffer(index, false); } catch (Throwable ignored) {}
                        } catch (Throwable t) {
                            Log.w(TAG, "onOutputBufferAvailable release failed: " + t);
                        }
                    }

                    @Override
                    public void onError(MediaCodec mc, MediaCodec.CodecException e) {
                        Log.w(TAG, "MediaCodec error: " + e);
                    }

                    @Override
                    public void onOutputFormatChanged(MediaCodec mc, MediaFormat format) {
                        Log.i(TAG, "Output format changed: " + format);
                    }
                });
            }

            // Configure codec to output to the ImageReader surface when possible to enable GPU-backed buffers.
            if (imageReader != null) {
                Surface surf = imageReader.getSurface();
                codec.configure(format, surf, null, 0);
            } else {
                codec.configure(format, null, null, 0);
            }
            codec.start();
            return id;
        } catch (Exception e) {
            Log.w(TAG, "createDecoder failed: " + e);
            return 0;
        }
    }

    // Called from Java when an AHardwareBuffer-backed image is available (API26+).
    private static native void onHardwareBufferAvailable(int decoderId, Object hardwareBuffer, long pts);

    private static byte[] ImageToNV12(Image image) {
        // Convert YUV_420_888 to NV12 (Y plane, interleaved UV)
        final Image.Plane[] planes = image.getPlanes();
        int w = image.getWidth();
        int h = image.getHeight();
        int ySize = w * h;
        int chromaWidth = (w + 1) / 2;
        int chromaHeight = (h + 1) / 2;
        int uvRowStrideOut = chromaWidth * 2;
        int uvSize = uvRowStrideOut * chromaHeight;
        byte[] out = new byte[ySize + uvSize];

        Image.Plane yPlane = planes[0];
        ByteBuffer yBuffer = yPlane.getBuffer().duplicate();
        int yRowStride = yPlane.getRowStride();
        int yPixelStride = yPlane.getPixelStride();
        for (int row = 0; row < h; row++) {
            int srcRow = row * yRowStride;
            int dstRow = row * w;
            for (int col = 0; col < w; col++) {
                out[dstRow + col] = yBuffer.get(srcRow + col * yPixelStride);
            }
        }

        Image.Plane uPlane = planes[1];
        Image.Plane vPlane = planes[2];
        ByteBuffer uBuffer = uPlane.getBuffer().duplicate();
        ByteBuffer vBuffer = vPlane.getBuffer().duplicate();
        int uRowStride = uPlane.getRowStride();
        int vRowStride = vPlane.getRowStride();
        int uPixelStride = uPlane.getPixelStride();
        int vPixelStride = vPlane.getPixelStride();

        int uvOffset = ySize;
        for (int row = 0; row < chromaHeight; row++) {
            int uRow = row * uRowStride;
            int vRow = row * vRowStride;
            int dstRow = uvOffset + row * uvRowStrideOut;
            for (int col = 0; col < chromaWidth; col++) {
                int dst = dstRow + col * 2;
                out[dst] = uBuffer.get(uRow + col * uPixelStride);
                out[dst + 1] = vBuffer.get(vRow + col * vPixelStride);
            }
        }
        return out;
    }

    // Native callback to deliver decoded frames to native code
    private static native void onFrameDecoded(int decoderId, byte[] data, int width, int height, long pts);

    // Called from native code to feed packet data to decoder
    public static boolean decode(int decoderId, byte[] packet, long pts) {
        MediaCodec codec = decoders.get(decoderId);
        if (codec == null) return false;
        try {
            int inputIndex = codec.dequeueInputBuffer(10000);
            if (inputIndex >= 0) {
                ByteBuffer inputBuf = codec.getInputBuffer(inputIndex);
                if (inputBuf == null) {
                    Log.w(TAG, "decode input buffer null");
                    codec.queueInputBuffer(inputIndex, 0, 0, pts, 0);
                    return false;
                }
                inputBuf.clear();
                inputBuf.put(packet);
                codec.queueInputBuffer(inputIndex, 0, packet.length, pts, 0);
            }
            return true;
        } catch (Exception e) {
            Log.w(TAG, "decode error: " + e);
            return false;
        }
    }

    public static void releaseDecoder(int decoderId) {
        MediaCodec codec = decoders.remove(decoderId);
        if (codec != null) {
            try { codec.stop(); } catch (Throwable ignored) {}
            try { codec.release(); } catch (Throwable ignored) {}
        }
    }
}
