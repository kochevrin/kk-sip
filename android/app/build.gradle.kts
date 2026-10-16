plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.plugin.compose")
}

// One version for every platform: taken from the desktop project() line.
val kksipVersion: String = Regex("""project\(kk-sip VERSION ([0-9.]+)""")
    .find(rootDir.resolve("../CMakeLists.txt").readText())!!.groupValues[1]
val kksipVersionCode: Int = kksipVersion.split('.').map { it.toInt() }
    .let { (major, minor, patch) -> major * 10000 + minor * 100 + patch }

// PJSIP built by android/scripts/build-pjsip.sh.
val pjsipRoot: String = System.getenv("KKSIP_DEPS")?.let { "$it/android/pjsip" }
    ?: rootDir.resolve("../.deps/android/pjsip").canonicalPath

android {
    namespace = "io.github.kochevrin.kksip"
    compileSdk = 37
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "io.github.kochevrin.kksip"
        minSdk = 26 // keep in sync with API_LEVEL in scripts/build-pjsip.sh
        targetSdk = 36
        versionCode = kksipVersionCode
        versionName = kksipVersion

        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_static", "-DKKSIP_PJSIP_ROOT=$pjsipRoot")
                cppFlags += listOf("-std=c++17", "-fexceptions", "-frtti")
            }
        }
    }

    // Release key from the environment (CI secrets, or a local env file kept outside the
    // repo). Without it the release APK is built unsigned.
    val keystore = System.getenv("KKSIP_KEYSTORE")
    signingConfigs {
        if (keystore != null) create("release") {
            storeFile = file(keystore)
            storePassword = System.getenv("KKSIP_KEYSTORE_PASSWORD")
            keyAlias = System.getenv("KKSIP_KEY_ALIAS")
            keyPassword = System.getenv("KKSIP_KEY_PASSWORD")
        }
    }

    buildTypes {
        debug {
            ndk { abiFilters += listOf("arm64-v8a", "x86_64") } // x86_64 for the emulator
        }
        release {
            ndk {
                abiFilters += listOf("arm64-v8a")
                if (project.hasProperty("withEmulatorAbi")) abiFilters += "x86_64" // ./gradlew -PwithEmulatorAbi
            }
            if (keystore != null) signingConfig = signingConfigs.getByName("release")
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "4.1.2"
        }
    }

    // Only the app's own languages; drops library translations from the APK.
    androidResources {
        localeFilters += listOf("en", "uk", "ru")
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    packaging {
        jniLibs { useLegacyPackaging = false }
    }
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2026.09.00")
    implementation(composeBom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui-tooling-preview")
    debugImplementation("androidx.compose.ui:ui-tooling")
    implementation("androidx.activity:activity-compose:1.13.0")
    implementation("androidx.core:core-ktx:1.19.1")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.11.0")
    implementation("androidx.lifecycle:lifecycle-process:2.11.0")
    implementation("androidx.core:core-telecom:1.0.1")
}
