# JNI: native methods and the callbacks libkksip.so calls by name.
-keepclasseswithmembernames class * { native <methods>; }
-keep class io.github.kochevrin.kksip.sip.SipListener { *; }
-keep class * implements io.github.kochevrin.kksip.sip.SipListener { *; }
