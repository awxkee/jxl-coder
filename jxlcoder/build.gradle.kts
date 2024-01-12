plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
    id("maven-publish")
}

//val androidSourcesJar by tasks.registering(Jar::class) {
//    archiveClassifier.set("sources")
//    from(android.sourceSets.named("main"))
//}

afterEvaluate {
    publishing {
        publications {
            create<MavenPublication>("mavenJava") {
                groupId = "com.github.awxkee"
                artifactId = "jxl-coder"
                version = "1.7.6"
                from(components["release"])
//                artifact(androidSourcesJar)
            }
        }
    }
}

android {
    namespace = "com.awxkee.jxlcoder"
    compileSdk = 34

    defaultConfig {
        minSdk = 21

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        consumerProguardFiles("consumer-rules.pro")
        externalNativeBuild {
            cmake {
                ndkVersion = "26.1.10909125"
                cppFlags.add ("-std=c++20")
                abiFilters += setOf("armeabi-v7a", "arm64-v8a", "x86_64", "x86")
            }
        }

        publishing {
            singleVariant("release") {
                withSourcesJar()
                withJavadocJar()
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    sourceSets.named("main") {
        this.jniLibs {
            this.srcDir("src/main/cpp/lib")
        }
    }

    externalNativeBuild {
        cmake {
            path("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.9.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.9.0")
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
}