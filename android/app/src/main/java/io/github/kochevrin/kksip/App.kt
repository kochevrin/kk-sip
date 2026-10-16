package io.github.kochevrin.kksip

import android.app.Application
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import io.github.kochevrin.kksip.sip.CallManager

class App : Application() {
    override fun onCreate() {
        super.onCreate()
        Notifications.createChannels(this)
        CallManager.init(this)
        ProcessLifecycleOwner.get().lifecycle.addObserver(object : DefaultLifecycleObserver {
            override fun onStart(owner: LifecycleOwner) = CallManager.onForeground()
            override fun onStop(owner: LifecycleOwner) = CallManager.onBackground()
        })
    }
}
