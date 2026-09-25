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

// This stable public sideload identity predates the repository split. The
// legacy file/alias names are retained solely so current debug builds can
// replace older development installs in place; they do not describe a mode.
val sideloadKeystoreSource = file("debug/lasso-dev.p12.b64")
// Keep the decoded key outside app/build: a combined `clean assembleDebug`
// deletes app/build after configuration and would otherwise erase the key
// before AGP validates the debug signing configuration.
val sideloadKeystoreFile = file(
    "${System.getProperty("java.io.tmpdir")}/analytic-continuation-lasso-dev.p12"
)
val sideloadSigningAvailable = sideloadKeystoreSource.isFile
if (sideloadSigningAvailable && !sideloadKeystoreFile.exists()) {
    sideloadKeystoreFile.parentFile.mkdirs()
    sideloadKeystoreFile.writeBytes(
        Base64.getDecoder().decode(sideloadKeystoreSource.readText().trim())
    )
}

val continuationFragmentOverride = System.getenv("ANALYTIC_CONTINUATION_FRAGMENT_OVERRIDE")
    ?.takeIf { it.isNotBlank() }
    ?.let(::file)
val wegertColorMarker = "/*__WEGERT_COLOR_CORE__*/"
val generatedWegertAssets = layout.buildDirectory.dir("generated/wegert-assets")
val assembleContinuationShader = tasks.register("assembleContinuationShader") {
    val template = file("src/main/assets/continuation.frag.in")
    val colorCore = file("src/main/assets/wegert_color.glsl")
    val output = generatedWegertAssets.map { it.file("continuation.frag") }

    inputs.files(template, colorCore)
    continuationFragmentOverride?.let { inputs.file(it) }
    outputs.file(output)

    doLast {
        val outputFile = output.get().asFile
        outputFile.parentFile.mkdirs()

        if (continuationFragmentOverride != null) {
            check(continuationFragmentOverride.isFile) {
                "ANALYTIC_CONTINUATION_FRAGMENT_OVERRIDE does not name a readable fragment"
            }
            outputFile.writeText(continuationFragmentOverride.readText())
        } else {
            val templateText = template.readText()
            check(templateText.contains(wegertColorMarker)) {
                "Continuation fragment template is missing the Wegert coloring-core marker"
            }
            check(templateText.indexOf(wegertColorMarker) == templateText.lastIndexOf(wegertColorMarker)) {
                "Continuation fragment template must contain exactly one Wegert coloring-core marker"
            }
            outputFile.writeText(templateText.replace(wegertColorMarker, colorCore.readText()))
        }
    }
}

android {
    namespace = "org.isomorphisms.analyticcontinuation"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "org.isomorphisms.analyticcontinuation"
        minSdk = 26
        targetSdk = 36
        versionCode = appVersionCode
        versionName = appVersionName
        manifestPlaceholders["appLabel"] = "Holomorphic Random Explorer"

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
            // Keep the legacy suffix so the corrected explorer can update the
            // older development APK already installed on test phones.
            applicationIdSuffix = ".lasso.dev"
            versionNameSuffix = "-dev"
            manifestPlaceholders["appLabel"] = "Holomorphic Random Explorer Dev"
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

    sourceSets {
        getByName("main") {
            // AGP source sets cannot consume Provider objects directly; the
            // explicit preBuild dependency below preserves generation order.
            assets.srcDir(generatedWegertAssets.get().asFile)
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

tasks.named("preBuild").configure {
    dependsOn(assembleContinuationShader)
}
