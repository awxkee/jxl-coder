import com.android.build.gradle.internal.tasks.factory.dependsOn
import com.vanniktech.maven.publish.AndroidMultiVariantLibrary

plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
    id("maven-publish")
    id("signing")
    id("com.vanniktech.maven.publish") version "0.28.0"
}

mavenPublishing {
    if (System.getenv("PUBLISH_STATE") == "Release") {
        signAllPublications()
    }
}

mavenPublishing {
    configure(
        AndroidMultiVariantLibrary(
        sourcesJar = true,
        publishJavadocJar = true,
    )
    )

    coordinates("io.github.awxkee", "jxl-coder", System.getenv("VERSION_NAME") ?: "0.0.10")

    pom {
        name.set("Jxl Coder")
        description.set("JPEG XL encoder/decoder for Android")
        inceptionYear.set("2023")
        url.set("https://github.com/awxkee/jxl-coder")
        licenses {
            license {
                name.set("The Apache License, Version 2.0")
                url.set("http://www.apache.org/licenses/LICENSE-2.0.txt")
                distribution.set("http://www.apache.org/licenses/LICENSE-2.0.txt")
            }
            license {
                name.set("The 3-Clause BSD License")
                url.set("https://opensource.org/license/bsd-3-clause")
                description.set("https://opensource.org/license/bsd-3-clause")
            }
        }
        developers {
            developer {
                id.set("awxkee")
                name.set("Radzivon Bartoshyk")
                url.set("https://github.com/awxkee")
                email.set("radzivon.bartoshyk@proton.me")
            }
        }
        scm {
            url.set("https://github.com/awxkee/jxl-coder")
            connection.set("scm:git:git@github.com:awxkee/jxl-coder.git")
            developerConnection.set("scm:git:ssh://git@github.com/awxkee/jxl-coder.git")
        }
    }
}

val preSyncTask by tasks.registering(Exec::class) {
    doFirst {
        workingDir = File(rootProject.rootDir, "jxlcoder")
        commandLine("bash", "download_deps.sh")
    }
}

tasks.register("DownloadDeps") {
    dependsOn(preSyncTask)
}

android {
    project.tasks.preBuild.dependsOn("DownloadDeps")
    namespace = "io.github.awxkee.jxlcoder"
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