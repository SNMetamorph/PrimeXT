package su.xash.primext;

import android.annotation.SuppressLint;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;
import androidx.coordinatorlayout.widget.CoordinatorLayout;

import com.android.volley.Request;
import com.android.volley.RequestQueue;
import com.android.volley.toolbox.JsonObjectRequest;
import com.android.volley.toolbox.Volley;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.floatingactionbutton.ExtendedFloatingActionButton;
import com.google.android.material.snackbar.Snackbar;
import com.google.android.material.textfield.TextInputEditText;

import org.json.JSONArray;
import org.json.JSONException;

public class LauncherActivity extends AppCompatActivity {
	
	@SuppressLint("SetTextI18n")
	@Override
	public void onCreate(Bundle savedInstanceBundle) {
		super.onCreate(savedInstanceBundle);
		setContentView(R.layout.activity_launcher);

		ExtractAssets.extractPAK(this, false);

		TextInputEditText launchParameters = findViewById(R.id.launchParameters);
		launchParameters.setText("-log -dev 2");

		ExtendedFloatingActionButton launchButton = findViewById(R.id.launchButton);
		launchButton.setOnClickListener((view) -> startActivity(new Intent().setComponent(new ComponentName("su.xash.engine", "su.xash.engine.XashActivity"))
				.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
				.putExtra("pakfile", getExternalFilesDir(null).getAbsolutePath() + "/extras.pak")
				.putExtra("gamedir", "primext")
				.putExtra("argv", launchParameters.getText())
				.putExtra("gamelibdir", getApplicationInfo().nativeLibraryDir)));

	}
}
