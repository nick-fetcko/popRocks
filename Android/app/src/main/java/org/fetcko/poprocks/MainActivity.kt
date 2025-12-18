package org.fetcko.poprocks

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.storage.StorageManager
import android.provider.DocumentsContract
import android.window.OnBackInvokedDispatcher
import androidx.core.net.toUri
import androidx.documentfile.provider.DocumentFile
import org.fetcko.poprocks.databinding.ActivityMainBinding
import java.io.File
import java.io.FileNotFoundException
import java.io.FileOutputStream
import java.net.URLDecoder

class MainActivity : org.libsdl.app.SDLActivity () {

	private lateinit var binding: ActivityMainBinding

	override fun onCreate(savedInstanceState: Bundle?) {
		super.onCreate(savedInstanceState)

		copyAssets("")
		val filesDir = getFilesDir()
		val sandboxPath = filesDir.absolutePath

		setPath(sandboxPath)
		setHdr(resources.configuration.isScreenHdr)
		setLibraryPath(applicationInfo.nativeLibraryDir)
		setFileOpenListener(this)

		onBackInvokedDispatcher.registerOnBackInvokedCallback(OnBackInvokedDispatcher.PRIORITY_DEFAULT, {
			val i = Intent()
			i.setAction(Intent.ACTION_MAIN)
			i.addCategory(Intent.CATEGORY_HOME)
			this.startActivity(i)
		});

		binding = ActivityMainBinding.inflate(layoutInflater)
	}

	private var stopped = false;
	override fun onStop() {
		super.onStop()

		destroyEglSurface()

		stopped = true;
	}

	override fun OnSurfaceDestroyed() {
		super.OnSurfaceDestroyed()
		destroyEglSurface()
	}

	override fun onWindowFocusChanged(isFocused : Boolean) {
		super.onWindowFocusChanged(isFocused)
		if (isFocused && stopped) createEglSurface()
		stopped = false
	}

	private fun copyFolder(uri: Uri, depth: Int, dir : DocumentFile? = null): File {
		val path = URLDecoder.decode(uri.toString()).split("/")
		var destinationPath = filesDir.absolutePath + "/current";

		for (i in 1..depth)
			destinationPath += "/" + path[path.size - i];

		val destinationDir = File(destinationPath)
		if (!destinationDir.exists())
			destinationDir.mkdirs()

		val pickedDir = dir ?: DocumentFile.fromTreeUri(this, uri)
		for (file in pickedDir?.listFiles()!!) {
			val outputFile = File(destinationDir, file.name)
			if (file.isDirectory && file != pickedDir) {
				outputFile.mkdirs()
				copyFolder(file.uri, depth + 1, file)
			}
			// Unless the files are registered with MediaStore
			// they aren't accessible to fopen / fstream / etc.
			//
			// On my phone only my Alarms are registered with MediaStore.
			// Poweramp (my music player of choice) doesn't register
			// my music since it uses Scoped Storage instead of
			// MediaStore.
			//
			// FIXME: To get around this we're copying the files to
			//        the application sandbox. This is incredibly
			//        inefficient.
			else {
				contentResolver.openInputStream(file.uri)?.use { inputStream ->
					val outputStream = FileOutputStream(outputFile)
					inputStream.copyTo(outputStream)
					outputStream.close()
				}
			}
		}

		return destinationDir
	}

	override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
		super.onActivityResult(requestCode, resultCode, data)

		val uri = data?.data
		if (requestCode == openRequestCode && resultCode == RESULT_OK && uri != null) {
			// Clear the old data
			File(filesDir.absolutePath + "/current").deleteRecursively()
			openFolder(copyFolder(uri, 0).absolutePath)
		}
	}

	val openRequestCode = 1;
	fun openDirectory() {
		val sm = baseContext.getSystemService(STORAGE_SERVICE) as StorageManager?
		val documentTreeIntent: Intent? = sm?.primaryStorageVolume?.createOpenDocumentTreeIntent()
		var uri = documentTreeIntent?.getParcelableExtra<Uri?>("android.provider.extra.INITIAL_URI")
		val startDir = "Music"
		var scheme = uri.toString()
		scheme = scheme.replace("/root/", "/document/")
		scheme += "%3A$startDir"
		uri = scheme.toUri()

		// Choose a directory using the system's file picker.
		val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).apply {
			// Optionally, specify a URI for the directory that should be opened in
			// the system file picker when it loads.
			putExtra(DocumentsContract.EXTRA_INITIAL_URI, uri)
		}

		startActivityForResult(intent, 1)
	}

	/**
	 * A native method that is implemented by the 'poprocks' native library,
	 * which is packaged with this application.
	 */
	external fun setPath(path : String)
	external fun setLibraryPath(path : String)
	external fun setFileOpenListener(activity: MainActivity)
	external fun createEglSurface()
	external fun destroyEglSurface()
	external fun openFolder(folder : String)
	external fun setHdr(hdr : Boolean)

	private fun copyAssets(path : String) {
		// Filter out paths we don't want
		if (setOf("geoid_map", "images", "webkit").contains(path))
			return

		val manager = baseContext?.assets;
		val assets = manager?.list(path)

		val destinationDir =
			if (path.isEmpty()) {
				filesDir.absolutePath + "/assets"
			} else {
				filesDir.absolutePath + "/assets/" + path
			}

		val destinationFile = File(destinationDir);

		if (!destinationFile.exists())
			destinationFile.mkdirs();

		if (assets != null) {
			for (asset in assets) {
				try {
					val inputStream =
						if (path.isEmpty()) { manager.open(asset) } else { manager.open("$path/$asset") }

					val file = File(destinationDir, asset)
					val outputStream = FileOutputStream(file)

					inputStream.copyTo(outputStream)

					outputStream.close()
				} catch (e : FileNotFoundException) {
					// If asset isn't a file, it's a subfolder
					copyAssets(asset);
				}
			}
		}
	}

	companion object {
		// Used to load the 'poprocks' library on application startup.
		init {
			System.loadLibrary("main")
		}
	}
}