package su.xash.primext;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import java.io.InputStream;
import java.io.File;
import java.io.FileOutputStream;

public class ExtractAssets {
	public static final String TAG = "ExtractAssets";

	private static void extractFile(Context context, String filename, boolean overwrite) {
		try {
			InputStream in = context.getAssets().open(filename);
			File outFile = new File(context.getExternalFilesDir(null), filename);

			if (outFile.isFile() && !overwrite) {
				return;
			}

			FileOutputStream out = new FileOutputStream(outFile);

			byte[] buffer = new byte[1024];
			int length;

			while ((length = in.read(buffer)) != -1) {
				out.write(buffer, 0, length);
			}

			in.close();
			out.flush();
			out.close();
		} catch (Exception e) {
			Log.e(TAG, "Failed to extract file" + filename + ":" + e);
		}
	}

	public static void extractPAK(Context context, boolean overwrite) {
		try {
			extractFile(context, "extras.pak", overwrite);
		} catch (Exception e) {
			Log.e(TAG, "Failed to extract PAK:" + e);
		}
	}
}
