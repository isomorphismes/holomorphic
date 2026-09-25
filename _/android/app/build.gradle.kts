plugins {
    id("com.android.application")
}

val requestedVersionCode = System.getenv("PLAY_VERSION_CODE")
val appVersionCode = when {
    requestedVersionCode == null -> 1
    else -> requestedVersionCode.toIntOrNull()
        ?.takeIf { it in 1..2_100_000_000 }
        ?: error("PLAY_VERSION_CODE must be between 1 and 2100000000")
}
val appVersionName = System.getenv("PLAY_VERSION_NAME") ?: "0.1.0"

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

val githubTestKeystore = file("analytic-continuation-github-test.p12")
val githubTestKeyPassword = "analytic-continuation-test"

android {
    namespace = "org.isomorphisms.analyticcontinuation"
    compileSdk = 36

    defaultConfig {
        applicationId = "org.isomorphisms.analyticcontinuation"
        minSdk = 26
        targetSdk = 36
        versionCode = appVersionCode
        versionName = appVersionName
    }

    signingConfigs {
        getByName("debug") {
            storeFile = githubTestKeystore
            storePassword = githubTestKeyPassword
            keyAlias = "analytic-continuation-test"
            keyPassword = githubTestKeyPassword
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
        getByName("release") {
            isMinifyEnabled = false
            signingConfigs.findByName("playUpload")?.let {
                signingConfig = it
            }
        }
    }
}
