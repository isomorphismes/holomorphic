import java.util.Base64

plugins {
    id("com.android.application")
}

// The defaults are the canonical tagged-source version used by F-Droid.
// Google Play may override them for an explicitly versioned store build.
val requestedVersionCode = System.getenv("PLAY_VERSION_CODE")
val appVersionCode = when {
    requestedVersionCode == null -> 3
    else -> requestedVersionCode.toIntOrNull()
        ?.takeIf { it in 1..2_100_000_000 }
        ?: error("PLAY_VERSION_CODE must be between 1 and 2100000000")
}
val appVersionName = System.getenv("PLAY_VERSION_NAME") ?: "0.2.1"

val uploadKeystorePath = System.getenv("ANDROID_UPLOAD_KEYSTORE_PATH")
val uploadKeystorePassword = System.getenv("ANDROID_UPLOAD_KEYSTORE_PASSWORD")
val uploadKeyAlias = System.getenv("ANDROID_UPLOAD_KEY_ALIAS")
val uploadKeyPassword = System.getenv("ANDROID_UPLOAD_KEY_PASSWORD")
val uploadSigningConfigured = listOf(
    uploadKeystorePath,
    uploadKeystorePassword,
    uploadKeyAlias,
    uploadKeyPassword,
).all { !it.isNullOrBlank() }

// Mainline keeps a stable, public Lasso Dev sideload key. F-Droid removes the
// encoded key before configuring Gradle, so release builds cannot depend on it.
val sideloadKeystoreSource = file("debug/lasso-dev.p12.b64")
val sideloadKeystoreFile = layout.buildDirectory.file("sideload-signing/lasso-dev.p12").get().asFile
val sideloadSigningAvailable = sideloadKeystoreSource.isFile
if (sideloadSigningAvailable && !sideloadKeystoreFile.exists()) {
    sideloadKeystoreFile.parentFile.mkdirs()
    sideloadKeystoreFile.writeBytes(
        Base64.getDecoder().decode(sideloadKeystoreSource.readText().trim())
    )
}

android {
    namespace = "org.isomorphisms.analyticcontinuation"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    defaultConfig {
        // Mainline/Play/F-Droid keep the established application identity.
        applicationId = "org.isomorphisms.analyticcontinuation"
        minSdk = 26
        targetSdk = 36
        versionCode = appVersionCode
        versionName = appVersionName
        manifestPlaceholders["appLabel"] = "Analytic Continuation — Lasso"

        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=none")
            }
        }
    }

    signingConfigs {
        if (sideloadSigningAvailable) {
            create("sideloadDev") {
                storeFile = sideloadKeystoreFile
                storePassword = "lasso-dev"
                keyAlias = "lasso-dev"
                keyPassword = "lasso-dev"
            }
        }

        if (uploadSigningConfigured) {
            create("playUpload") {
                storeFile = file(uploadKeystorePath!!)
                storePassword = uploadKeystorePassword
                keyAlias = uploadKeyAlias
                keyPassword = uploadKeyPassword
            }
        }
    }

    buildTypes {
        getByName("debug") {
            // Preserve the permanent Lasso Dev sideload/update channel when its
            // public key input is present. F-Droid does not need a debug signer.
            applicationIdSuffix = ".lasso.dev"
            versionNameSuffix = "-dev"
            manifestPlaceholders["appLabel"] = "Analytic Continuation — Lasso Dev"
            signingConfigs.findByName("sideloadDev")?.let {
                signingConfig = it
            }
        }

        getByName("release") {
            isMinifyEnabled = false
            signingConfigs.findByName("playUpload")?.let {
                signingConfig = it
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}
