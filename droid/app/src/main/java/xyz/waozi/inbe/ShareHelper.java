package xyz.waozi.inbe;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;
import androidx.core.content.FileProvider;
import java.io.File;
import java.io.FileOutputStream;

public class ShareHelper {
    private static final String TAG = "InbeShare";

    public static void shareZipFile(Activity activity, byte[] zipData, String filename, String chooserTitle) {
        if (activity == null || zipData == null) return;

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    File cacheFile = new File(activity.getCacheDir(), filename);
                    FileOutputStream fos = new FileOutputStream(cacheFile);
                    fos.write(zipData);
                    fos.close();

                    Uri contentUri = FileProvider.getUriForFile(
                        activity,
                        "xyz.waozi.inbe.fileprovider",
                        cacheFile
                    );

                    Intent sendIntent = new Intent(Intent.ACTION_SEND);
                    sendIntent.setType("application/zip");
                    sendIntent.putExtra(Intent.EXTRA_STREAM, contentUri);
                    sendIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

                    Intent shareIntent = Intent.createChooser(sendIntent, chooserTitle);
                    activity.startActivity(shareIntent);

                    Log.i(TAG, "Share sheet shown");
                } catch (Exception e) {
                    Log.e(TAG, "Share failed", e);
                }
            }
        });
    }
}
