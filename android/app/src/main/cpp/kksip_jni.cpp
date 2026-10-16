// JNI bridge between the Kotlin app and pjsua2.
//
// Every entry point is called from one Kotlin thread (SipThread), the same thread that
// created the endpoint, so PJSIP callbacks also arrive on it (threadCnt = 0, events are
// pumped by handleEvents()). Nothing here needs locking.

#include <jni.h>
#include <android/log.h>

#include <pjsua2.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

const char *TAG = "kksip";

JavaVM *g_vm = nullptr;
jobject g_listener = nullptr; // SipListener, global ref
jmethodID g_onCallState = nullptr;
jmethodID g_onCallMedia = nullptr;
jmethodID g_onRegState = nullptr;
jmethodID g_onIncomingCall = nullptr;
jmethodID g_onTransferStatus = nullptr;
std::string g_lastError;

std::string toStd(JNIEnv *env, jstring s)
{
    if (!s)
        return {};
    // Standard UTF-8 (GetStringUTFChars gives modified UTF-8, wrong for emoji).
    const jsize len = env->GetStringLength(s);
    std::u16string u(size_t(len), u'\0');
    env->GetStringRegion(s, 0, len, reinterpret_cast<jchar *>(u.data()));
    std::string out;
    out.reserve(u.size());
    for (size_t i = 0; i < u.size(); ++i) {
        char32_t cp = u[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < u.size() && u[i + 1] >= 0xDC00 && u[i + 1] <= 0xDFFF)
            cp = 0x10000 + ((cp - 0xD800) << 10) + (u[++i] - 0xDC00);
        if (cp < 0x80) {
            out += char(cp);
        } else if (cp < 0x800) {
            out += char(0xC0 | cp >> 6);
            out += char(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += char(0xE0 | cp >> 12);
            out += char(0x80 | (cp >> 6 & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        } else {
            out += char(0xF0 | cp >> 18);
            out += char(0x80 | (cp >> 12 & 0x3F));
            out += char(0x80 | (cp >> 6 & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

JNIEnv *jniEnv()
{
    JNIEnv *env = nullptr;
    g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    return env;
}

// SIP text is UTF-8 (caller names are often Cyrillic). NewStringUTF wants modified UTF-8
// and aborts the VM on bad input, so decode by hand into UTF-16; invalid bytes become U+FFFD.
jstring toJava(JNIEnv *env, const std::string &s)
{
    std::u16string out;
    out.reserve(s.size());
    const auto *p = reinterpret_cast<const unsigned char *>(s.data());
    const size_t n = s.size();
    for (size_t i = 0; i < n;) {
        unsigned char c = p[i];
        char32_t cp = 0xFFFD;
        size_t len = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < n && (p[i + 1] & 0xC0) == 0x80) {
            cp = char32_t(c & 0x1F) << 6 | (p[i + 1] & 0x3F);
            len = 2;
            if (cp < 0x80) cp = 0xFFFD;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < n && (p[i + 1] & 0xC0) == 0x80
                   && (p[i + 2] & 0xC0) == 0x80) {
            cp = char32_t(c & 0x0F) << 12 | char32_t(p[i + 1] & 0x3F) << 6 | (p[i + 2] & 0x3F);
            len = 3;
            if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < n && (p[i + 1] & 0xC0) == 0x80
                   && (p[i + 2] & 0xC0) == 0x80 && (p[i + 3] & 0xC0) == 0x80) {
            cp = char32_t(c & 0x07) << 18 | char32_t(p[i + 1] & 0x3F) << 12
                | char32_t(p[i + 2] & 0x3F) << 6 | (p[i + 3] & 0x3F);
            len = 4;
            if (cp < 0x10000 || cp > 0x10FFFF) cp = 0xFFFD;
        }
        if (cp >= 0x10000) {
            cp -= 0x10000;
            out += char16_t(0xD800 + (cp >> 10));
            out += char16_t(0xDC00 + (cp & 0x3FF));
        } else {
            out += char16_t(cp);
        }
        i += len;
    }
    return env->NewString(reinterpret_cast<const jchar *>(out.data()), jsize(out.size()));
}

class AndroidLog : public pj::LogWriter {
public:
    void write(const pj::LogEntry &e) override
    {
        const int prio = e.level <= 1 ? ANDROID_LOG_ERROR
            : e.level == 2            ? ANDROID_LOG_WARN
            : e.level <= 4            ? ANDROID_LOG_INFO
                                      : ANDROID_LOG_DEBUG;
        __android_log_write(prio, TAG, e.msg.c_str());
    }
};

class KkCall;
class KkAccount;

struct Engine {
    std::unique_ptr<pj::Endpoint> ep;
    std::map<std::string, std::unique_ptr<KkAccount>> accounts;
    std::map<int, KkCall *> calls;
    std::vector<KkCall *> dead; // disconnected, deleted on the next handleEvents()
};
Engine *g_engine = nullptr;

class KkAccount : public pj::Account {
public:
    explicit KkAccount(std::string id) : id(std::move(id)) {}

    const std::string id;
    bool registering = false; // registration wanted: incoming calls are accepted

    void onRegState(pj::OnRegStateParam &prm) override;
    void onIncomingCall(pj::OnIncomingCallParam &prm) override;
};

class KkCall : public pj::Call {
public:
    explicit KkCall(pj::Account &acc, int callId = PJSUA_INVALID_ID) : pj::Call(acc, callId) {}

    bool muted = false;

    void onCallState(pj::OnCallStateParam &) override
    {
        pj::CallInfo ci;
        try {
            ci = getInfo();
        } catch (const pj::Error &) {
            return;
        }
        JNIEnv *env = jniEnv();
        if (env && g_listener) {
            jstring reason = toJava(env, ci.lastReason);
            jstring remote = toJava(env, ci.remoteUri);
            env->CallVoidMethod(g_listener, g_onCallState, jint(ci.id), jint(ci.state),
                                jint(ci.lastStatusCode), reason, remote);
            env->DeleteLocalRef(reason);
            env->DeleteLocalRef(remote);
        }
        if (ci.state == PJSIP_INV_STATE_DISCONNECTED && g_engine) {
            g_engine->calls.erase(ci.id);
            g_engine->dead.push_back(this);
        }
    }

    void onCallMediaState(pj::OnCallMediaStateParam &) override { connectAudio(); }

    // Blind transfer: the far end reports progress with NOTIFYs. Once it has the new
    // party (2xx), our leg is done; hang it up like the desktop does.
    void onCallTransferStatus(pj::OnCallTransferStatusParam &prm) override
    {
        JNIEnv *env = jniEnv();
        if (env && g_listener)
            env->CallVoidMethod(g_listener, g_onTransferStatus, jint(getId()), jint(prm.statusCode),
                                jboolean(prm.finalNotify));
        if (prm.finalNotify && int(prm.statusCode) / 100 == 2) {
            prm.cont = false;
            pj::CallOpParam op(true);
            try {
                hangup(op);
            } catch (const pj::Error &) {
            }
        }
    }

    void connectAudio()
    {
        pj::CallInfo ci;
        try {
            ci = getInfo();
        } catch (const pj::Error &) {
            return;
        }
        bool active = false;
        bool localHold = false;
        for (unsigned i = 0; i < ci.media.size(); ++i) {
            const pj::CallMediaInfo &mi = ci.media[i];
            if (mi.type != PJMEDIA_TYPE_AUDIO)
                continue;
            if (mi.status == PJSUA_CALL_MEDIA_LOCAL_HOLD)
                localHold = true;
            if (mi.status != PJSUA_CALL_MEDIA_ACTIVE && mi.status != PJSUA_CALL_MEDIA_REMOTE_HOLD)
                continue;
            try {
                pj::AudioMedia am = getAudioMedia(int(i));
                pj::AudDevManager &mgr = g_engine->ep->audDevManager();
                am.startTransmit(mgr.getPlaybackDevMedia());
                if (muted)
                    mgr.getCaptureDevMedia().stopTransmit(am);
                else
                    mgr.getCaptureDevMedia().startTransmit(am);
                active = true;
            } catch (const pj::Error &e) {
                __android_log_print(ANDROID_LOG_WARN, TAG, "audio connect: %s", e.info().c_str());
            }
        }
        JNIEnv *env = jniEnv();
        if (env && g_listener)
            env->CallVoidMethod(g_listener, g_onCallMedia, jint(ci.id), jboolean(active),
                                jboolean(localHold));
    }
};

KkCall *findCall(int id)
{
    if (!g_engine)
        return nullptr;
    auto it = g_engine->calls.find(id);
    return it == g_engine->calls.end() ? nullptr : it->second;
}

void KkAccount::onRegState(pj::OnRegStateParam &prm)
{
    int expires = 0;
    try {
        pj::AccountInfo ai = getInfo();
        expires = ai.regIsActive ? int(ai.regExpiresSec) : 0;
    } catch (const pj::Error &) {
    }
    JNIEnv *env = jniEnv();
    if (env && g_listener) {
        jstring jid = toJava(env, id);
        jstring reason = toJava(env, prm.reason);
        env->CallVoidMethod(g_listener, g_onRegState, jid, jint(prm.code), reason, jint(expires));
        env->DeleteLocalRef(jid);
        env->DeleteLocalRef(reason);
    }
}

void KkAccount::onIncomingCall(pj::OnIncomingCallParam &prm)
{
    if (!g_engine || !registering) {
        // Nothing asked for calls on this account (e.g. a stray INVITE): refuse it.
        pjsua_call_hangup(prm.callId, PJSIP_SC_DECLINE, nullptr, nullptr);
        return;
    }
    auto *call = new KkCall(*this, prm.callId);
    g_engine->calls[prm.callId] = call;

    std::string remote;
    try {
        remote = call->getInfo().remoteUri;
    } catch (const pj::Error &) {
    }
    JNIEnv *env = jniEnv();
    if (env && g_listener) {
        jstring jid = toJava(env, id);
        jstring jremote = toJava(env, remote);
        env->CallVoidMethod(g_listener, g_onIncomingCall, jint(prm.callId), jid, jremote);
        env->DeleteLocalRef(jid);
        env->DeleteLocalRef(jremote);
    }
    // Unless the app already answered or refused it, tell the caller we are ringing.
    if (findCall(prm.callId) == call) {
        try {
            pj::CallInfo ci = call->getInfo();
            if (ci.state == PJSIP_INV_STATE_INCOMING || ci.state == PJSIP_INV_STATE_EARLY) {
                pj::CallOpParam ring;
                ring.statusCode = PJSIP_SC_RINGING;
                call->answer(ring);
            }
        } catch (const pj::Error &e) {
            __android_log_print(ANDROID_LOG_WARN, TAG, "ringing: %s", e.info().c_str());
        }
    }
}

void reapDeadCalls()
{
    if (!g_engine)
        return;
    for (KkCall *c : g_engine->dead)
        delete c;
    g_engine->dead.clear();
}

std::string withTransport(std::string uri, const std::string &transport)
{
    if (transport != "udp" && uri.find(";transport=") == std::string::npos)
        uri += ";transport=" + transport;
    return uri;
}

void shutdownEngine()
{
    if (!g_engine)
        return;
    try {
        g_engine->ep->hangupAllCalls();
    } catch (const pj::Error &) {
    }
    for (auto &kv : g_engine->calls)
        g_engine->dead.push_back(kv.second);
    g_engine->calls.clear();
    reapDeadCalls();
    g_engine->accounts.clear();
    try {
        g_engine->ep->libDestroy();
    } catch (const pj::Error &) {
    }
    delete g_engine;
    g_engine = nullptr;
}

} // namespace

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *)
{
    g_vm = vm;
    pj_jni_set_jvm(vm); // the Android audio device reaches AudioRecord/AudioTrack through it
    return JNI_VERSION_1_6;
}

// Returns null on success or the error text.
JNIEXPORT jstring JNICALL
Java_io_github_kochevrin_kksip_sip_Native_start(JNIEnv *env, jobject, jstring userAgent,
                                                jobject listener, jboolean debugLog, jint sipPort)
{
    if (g_engine)
        return nullptr;

    if (g_listener)
        env->DeleteGlobalRef(g_listener);
    g_listener = env->NewGlobalRef(listener);
    jclass cls = env->GetObjectClass(listener);
    g_onCallState = env->GetMethodID(cls, "onCallState", "(IIILjava/lang/String;Ljava/lang/String;)V");
    g_onCallMedia = env->GetMethodID(cls, "onCallMedia", "(IZZ)V");
    g_onRegState = env->GetMethodID(cls, "onRegState", "(Ljava/lang/String;ILjava/lang/String;I)V");
    g_onIncomingCall = env->GetMethodID(cls, "onIncomingCall", "(ILjava/lang/String;Ljava/lang/String;)V");
    g_onTransferStatus = env->GetMethodID(cls, "onTransferStatus", "(IIZ)V");
    env->DeleteLocalRef(cls);

    auto *engine = new Engine;
    try {
        engine->ep = std::make_unique<pj::Endpoint>();
        engine->ep->libCreate();
        // Same as the desktop: never switch a big INVITE to TCP behind the PBX's back.
        pjsip_cfg()->endpt.disable_tcp_switch = PJ_TRUE;

        pj::EpConfig cfg;
        cfg.uaConfig.userAgent = toStd(env, userAgent);
        cfg.uaConfig.threadCnt = 0;
        cfg.uaConfig.mainThreadOnly = false;
        cfg.uaConfig.maxCalls = 4;
        cfg.medConfig.noVad = true;
        cfg.logConfig.writer = new AndroidLog; // owned by the endpoint
        cfg.logConfig.consoleLevel = debugLog ? 5 : 2;
        cfg.logConfig.level = debugLog ? 5 : 2;
        cfg.logConfig.msgLogging = debugLog;
        cfg.logConfig.decor = 0;
        engine->ep->libInit(cfg);

        pj::TransportConfig tcfg;
        tcfg.port = unsigned(sipPort); // 0 = random
        engine->ep->transportCreate(PJSIP_TRANSPORT_UDP, tcfg);
        try {
            engine->ep->transportCreate(PJSIP_TRANSPORT_TCP, tcfg);
        } catch (const pj::Error &e) {
            __android_log_print(ANDROID_LOG_WARN, TAG, "TCP transport: %s", e.info().c_str());
        }
        engine->ep->libStart();
    } catch (const pj::Error &e) {
        g_engine = engine;
        shutdownEngine();
        return toJava(env, e.reason.empty() ? e.info() : e.reason);
    }
    g_engine = engine;
    return nullptr;
}

JNIEXPORT void JNICALL
Java_io_github_kochevrin_kksip_sip_Native_stop(JNIEnv *env, jobject)
{
    shutdownEngine();
    if (g_listener) {
        env->DeleteGlobalRef(g_listener);
        g_listener = nullptr;
    }
}

JNIEXPORT void JNICALL
Java_io_github_kochevrin_kksip_sip_Native_handleEvents(JNIEnv *, jobject, jint msec)
{
    if (!g_engine)
        return;
    reapDeadCalls();
    g_engine->ep->libHandleEvents(unsigned(msec));
}

// Adds or replaces an account. With doRegister the account registers and takes incoming
// calls; without it, it only gives outgoing calls their From identity, credentials and
// route. Returns null on success or the error text.
JNIEXPORT jstring JNICALL
Java_io_github_kochevrin_kksip_sip_Native_setAccount(
    JNIEnv *env, jobject, jstring jid, jstring jserver, jstring jdomain, jstring jproxy,
    jstring juser, jstring jauthUser, jstring jpassword, jstring jdisplayName,
    jstring jtransport, jboolean srtp, jboolean natRewrite, jboolean doRegister, jint regExpiry)
{
    if (!g_engine)
        return toJava(env, "SIP engine not running");

    const std::string id = toStd(env, jid);
    const std::string server = toStd(env, jserver);
    const std::string domain = toStd(env, jdomain).empty() ? server : toStd(env, jdomain);
    const std::string proxy = toStd(env, jproxy);
    const std::string user = toStd(env, juser);
    const std::string authUser = toStd(env, jauthUser).empty() ? user : toStd(env, jauthUser);
    const std::string displayName = toStd(env, jdisplayName);
    std::string transport = toStd(env, jtransport);
    if (transport.empty())
        transport = "udp";
    if (transport != "udp" && transport != "tcp")
        return toJava(env, "Transport " + transport + " is not supported on Android");

    pj::AccountConfig c;
    std::string idUri = "<sip:" + user + "@" + domain + ">";
    if (!displayName.empty()) {
        std::string name;
        for (char ch : displayName)
            if (ch != '"')
                name += ch;
        idUri = "\"" + name + "\" " + idUri;
    }
    c.idUri = idUri;
    if (doRegister) {
        c.regConfig.registrarUri = withTransport("sip:" + server, transport);
        c.regConfig.registerOnAdd = true;
        c.regConfig.timeoutSec = unsigned(regExpiry < 60 ? 60 : regExpiry);
        c.regConfig.retryIntervalSec = 30;
        c.regConfig.firstRetryIntervalSec = 5;
    } else {
        c.regConfig.registrarUri = "";
        c.regConfig.registerOnAdd = false;
    }
    c.sipConfig.authCreds.push_back(pj::AuthCredInfo("digest", "*", authUser, 0, toStd(env, jpassword)));

    // Calls go to sip:number@domain. Without a proxy, route them to the server whenever
    // the domain is a different name, so the PBX gets them like REGISTER does.
    std::string route = proxy;
    if (route.empty() && domain != server && !doRegister)
        route = server;
    if (!route.empty()) {
        if (route.rfind("sip:", 0) != 0)
            route = "sip:" + route;
        route = withTransport(route, transport);
        if (route.find(";lr") == std::string::npos)
            route += ";lr";
        c.sipConfig.proxies.push_back(route);
    }

    c.natConfig.viaRewriteUse = natRewrite;
    c.natConfig.sdpNatRewriteUse = natRewrite;
    c.natConfig.contactRewriteUse = natRewrite ? 2 : 0;
    c.natConfig.sipOutboundUse = 0;
    c.mediaConfig.srtpUse = srtp ? PJMEDIA_SRTP_OPTIONAL : PJMEDIA_SRTP_DISABLED;
    c.mediaConfig.srtpSecureSignaling = 0;
    c.presConfig.publishEnabled = false;

    try {
        auto it = g_engine->accounts.find(id);
        if (it != g_engine->accounts.end()) {
            KkAccount *acc = it->second.get();
            const bool wasRegistering = acc->registering;
            acc->registering = doRegister;
            acc->modify(c);
            if (wasRegistering && !doRegister)
                acc->setRegistration(false);
        } else {
            auto acc = std::make_unique<KkAccount>(id);
            acc->registering = doRegister;
            acc->create(c);
            g_engine->accounts[id] = std::move(acc);
        }
    } catch (const pj::Error &e) {
        return toJava(env, e.reason.empty() ? e.info() : e.reason);
    }
    return nullptr;
}

JNIEXPORT void JNICALL
Java_io_github_kochevrin_kksip_sip_Native_removeAccount(JNIEnv *env, jobject, jstring jid)
{
    if (g_engine)
        g_engine->accounts.erase(toStd(env, jid));
}

// Returns the call id, or -1 with lastError() set.
JNIEXPORT jint JNICALL
Java_io_github_kochevrin_kksip_sip_Native_makeCall(JNIEnv *env, jobject, jstring jaccount,
                                                   jstring jtarget)
{
    g_lastError.clear();
    if (!g_engine) {
        g_lastError = "SIP engine not running";
        return -1;
    }
    auto it = g_engine->accounts.find(toStd(env, jaccount));
    if (it == g_engine->accounts.end()) {
        g_lastError = "No account";
        return -1;
    }
    auto *call = new KkCall(*it->second);
    pj::CallOpParam prm(true);
    prm.opt.audioCount = 1;
    prm.opt.videoCount = 0;
    prm.opt.textCount = 0;
    try {
        call->makeCall(toStd(env, jtarget), prm);
    } catch (const pj::Error &e) {
        g_lastError = e.reason.empty() ? e.info() : e.reason;
        delete call;
        return -1;
    }
    const int id = call->getId();
    // A call that failed synchronously is already queued for deletion by onCallState.
    bool dead = false;
    for (KkCall *d : g_engine->dead)
        dead = dead || d == call;
    if (!dead)
        g_engine->calls[id] = call;
    return id;
}

JNIEXPORT jstring JNICALL
Java_io_github_kochevrin_kksip_sip_Native_lastError(JNIEnv *env, jobject)
{
    return toJava(env, g_lastError);
}

// code: 0 for a normal hang-up, 486 busy or 603 decline for a call still ringing.
JNIEXPORT void JNICALL
Java_io_github_kochevrin_kksip_sip_Native_hangup(JNIEnv *, jobject, jint id, jint code)
{
    if (KkCall *call = findCall(id)) {
        pj::CallOpParam prm(true);
        if (code > 0)
            prm.statusCode = pjsip_status_code(code);
        try {
            call->hangup(prm);
        } catch (const pj::Error &e) {
            __android_log_print(ANDROID_LOG_WARN, TAG, "hangup: %s", e.info().c_str());
        }
    }
}

JNIEXPORT void JNICALL
Java_io_github_kochevrin_kksip_sip_Native_answer(JNIEnv *, jobject, jint id)
{
    if (KkCall *call = findCall(id)) {
        pj::CallOpParam prm;
        prm.statusCode = PJSIP_SC_OK;
        prm.opt.audioCount = 1;
        prm.opt.videoCount = 0;
        prm.opt.textCount = 0;
        try {
            call->answer(prm);
        } catch (const pj::Error &e) {
            __android_log_print(ANDROID_LOG_WARN, TAG, "answer: %s", e.info().c_str());
        }
    }
}

// Re-sends REGISTER now: after the phone woke up, before the registration runs out.
JNIEXPORT void JNICALL
Java_io_github_kochevrin_kksip_sip_Native_renewRegistration(JNIEnv *env, jobject, jstring jid)
{
    if (!g_engine)
        return;
    auto it = g_engine->accounts.find(toStd(env, jid));
    if (it == g_engine->accounts.end() || !it->second->registering)
        return;
    try {
        it->second->setRegistration(true);
    } catch (const pj::Error &e) {
        __android_log_print(ANDROID_LOG_WARN, TAG, "register: %s", e.info().c_str());
    }
}

// The phone moved to another network (Wi-Fi <-> mobile, VPN up/down): re-register and
// keep calls going on the new address.
JNIEXPORT void JNICALL
Java_io_github_kochevrin_kksip_sip_Native_ipChanged(JNIEnv *, jobject)
{
    if (!g_engine)
        return;
    try {
        pj::IpChangeParam prm;
        g_engine->ep->handleIpChange(prm);
    } catch (const pj::Error &e) {
        __android_log_print(ANDROID_LOG_WARN, TAG, "ip change: %s", e.info().c_str());
    }
}

JNIEXPORT void JNICALL
Java_io_github_kochevrin_kksip_sip_Native_setMute(JNIEnv *, jobject, jint id, jboolean mute)
{
    if (KkCall *call = findCall(id)) {
        call->muted = mute;
        call->connectAudio();
    }
}

JNIEXPORT void JNICALL
Java_io_github_kochevrin_kksip_sip_Native_setHold(JNIEnv *, jobject, jint id, jboolean hold)
{
    KkCall *call = findCall(id);
    if (!call)
        return;
    pj::CallOpParam prm(true);
    try {
        if (hold) {
            call->setHold(prm);
        } else {
            prm.opt.flag = PJSUA_CALL_UNHOLD;
            prm.opt.videoCount = 0;
            prm.opt.textCount = 0;
            call->reinvite(prm);
        }
    } catch (const pj::Error &e) {
        __android_log_print(ANDROID_LOG_WARN, TAG, "hold: %s", e.info().c_str());
    }
}

// Blind transfer to target (a SIP URI). Returns null when the REFER went out.
JNIEXPORT jstring JNICALL
Java_io_github_kochevrin_kksip_sip_Native_transfer(JNIEnv *env, jobject, jint id, jstring target)
{
    KkCall *call = findCall(id);
    if (!call)
        return toJava(env, "No call");
    pj::CallOpParam prm(true);
    try {
        call->xfer(toStd(env, target), prm);
    } catch (const pj::Error &e) {
        return toJava(env, e.reason.empty() ? e.info() : e.reason);
    }
    return nullptr;
}

JNIEXPORT void JNICALL
Java_io_github_kochevrin_kksip_sip_Native_dtmf(JNIEnv *env, jobject, jint id, jstring digits)
{
    if (KkCall *call = findCall(id)) {
        try {
            call->dialDtmf(toStd(env, digits));
        } catch (const pj::Error &e) {
            __android_log_print(ANDROID_LOG_WARN, TAG, "dtmf: %s", e.info().c_str());
        }
    }
}

} // extern "C"
