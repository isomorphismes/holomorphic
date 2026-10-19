import java.util.Base64
import java.security.MessageDigest

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
val sideloadKeystoreFile = layout.buildDirectory.file("sideload-signing/lasso-dev.p12").get().asFile
val sideloadSigningAvailable = sideloadKeystoreSource.isFile
if (sideloadSigningAvailable && !sideloadKeystoreFile.exists()) {
    sideloadKeystoreFile.parentFile.mkdirs()
    sideloadKeystoreFile.writeBytes(
        Base64.getDecoder().decode(sideloadKeystoreSource.readText().trim())
    )
}

val wegertColorMarker = "/*__WEGERT_COLOR_CORE__*/"
val generatedWegertAssets = layout.buildDirectory.dir("generated/wegert-assets")
val shaderBindingHeader = rootProject.projectDir.parentFile.resolve("_/build/shader-binding/holomorphic_shader_binding.h")
val requestedCheckedFragment = System.getenv("HOLOMORPHIC_GENERATED_FRAGMENT")
val assembleContinuationShader = tasks.register("assembleContinuationShader") {
    val template = file("src/main/assets/continuation.frag.in")
    val colorCore = file("src/main/assets/wegert_color.glsl")
    val checkedFragment = requestedCheckedFragment?.let { file(it) }
    val checkedReceipt = checkedFragment?.resolveSibling("continuation.provenance")
    val output = generatedWegertAssets.map { it.file("continuation.frag") }
    val receiptOutput = generatedWegertAssets.map { it.file("continuation.provenance") }

    inputs.property("shaderMode", if (checkedFragment == null) "legacy" else "checked-idris")
    if (checkedFragment == null) {
        inputs.files(template, colorCore)
    } else {
        inputs.files(checkedFragment, checkedReceipt)
    }
    outputs.dir(generatedWegertAssets)
    outputs.file(shaderBindingHeader)

    doLast {
        val outputFile = output.get().asFile
        val outputReceipt = receiptOutput.get().asFile
        outputFile.parentFile.mkdirs()
        shaderBindingHeader.parentFile.mkdirs()

        if (checkedFragment != null) {
            check(checkedFragment.isFile) { "Requested compiled Idris fragment is absent" }
            check(checkedReceipt != null && checkedReceipt.isFile) {
                "Compiled shader has no exact-source/backend provenance"
            }
            val checkedBytes = checkedFragment.readBytes()
            check(checkedBytes.isNotEmpty() && checkedBytes.none { it == 0.toByte() }) {
                "Compiled fragment must be a nonempty text byte sequence without NUL"
            }
            val checkedText = checkedBytes.toString(Charsets.UTF_8)
            check(checkedText.contains("uniform vec2 u_holomorphic_coefficients[5];")) {
                "Compiled Holomorphic fragment lost the five complex coefficients"
            }
            check(checkedText.contains("u_remote_pole_time") && checkedText.contains("void main()")) {
                "Compiled Holomorphic fragment lost motion or its entry point"
            }
            check(!checkedText.contains(wegertColorMarker)) {
                "Generated mode cannot consume the legacy handwritten shader template"
            }
            val receiptLines = checkedReceipt.readLines()
            fun exactReceiptField(key: String): String {
                val fields = receiptLines.filter { it.startsWith("$key=") }
                check(fields.size == 1) { "Generated fragment needs exactly one $key receipt field" }
                return fields.single().substringAfter('=')
            }
            check(exactReceiptField("compiler") == "idris2-glsles")
            check(exactReceiptField("backend_repo") == "fuego-ironworks/idris-shader-backend")
            check(exactReceiptField("application_repo") == "isomorphismes/holomorphic")
            val backendCommit = exactReceiptField("backend_commit")
            val applicationCommit = exactReceiptField("application_commit")
            check(backendCommit.matches(Regex("[0-9a-f]{40}")))
            check(applicationCommit.matches(Regex("[0-9a-f]{40}")))
            val digest = MessageDigest.getInstance("SHA-256").digest(checkedBytes)
                .joinToString("") { "%02x".format(it.toInt() and 255) }
            check(exactReceiptField("fragment_sha256") == digest) {
                "Compiled fragment bytes differ from the generation receipt"
            }
            // Keep the generated bytes exact; text decoding never rewrites them.
            outputFile.writeBytes(checkedBytes)
            checkedReceipt.copyTo(outputReceipt, overwrite = true)
            shaderBindingHeader.writeText(
                "#define HOLOMORPHIC_REQUIRE_GENERATED_SHADER 1\n" +
                "#define HOLOMORPHIC_FRAGMENT_SHA256 \"$digest\"\n" +
                "#define HOLOMORPHIC_FRAGMENT_LENGTH ${checkedBytes.size}u\n" +
                "#define HOLOMORPHIC_BACKEND_COMMIT \"$backendCommit\"\n" +
                "#define HOLOMORPHIC_APPLICATION_COMMIT \"$applicationCommit\"\n"
            )
        } else {
            val templateText = template.readText()
            check(templateText.contains(wegertColorMarker)) {
                "Continuation fragment template is missing the Wegert coloring-core marker"
            }
            check(templateText.indexOf(wegertColorMarker) == templateText.lastIndexOf(wegertColorMarker)) {
                "Continuation fragment template must contain exactly one Wegert coloring-core marker"
            }
            outputFile.writeText(templateText.replace(wegertColorMarker, colorCore.readText()))
            outputReceipt.delete()
            shaderBindingHeader.writeText(
                "#define HOLOMORPHIC_REQUIRE_GENERATED_SHADER 0\n" +
                "#define HOLOMORPHIC_FRAGMENT_SHA256 \"\"\n" +
                "#define HOLOMORPHIC_FRAGMENT_LENGTH 0u\n" +
                "#define HOLOMORPHIC_BACKEND_COMMIT \"\"\n" +
                "#define HOLOMORPHIC_APPLICATION_COMMIT \"\"\n"
            )
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
