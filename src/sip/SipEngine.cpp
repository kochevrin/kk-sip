#include "sip/SipEngine.h"

#include "core/SipUri.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QSet>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include <pjsua2.hpp>

#include <algorithm>

Q_LOGGING_CATEGORY(lcBlf, "kksip.blf", QtWarningMsg) // QT_LOGGING_RULES="kksip.blf.debug=true"

namespace {

std::string toStd(const QString &s)
{
    return s.toStdString();
}

QString fromStd(const std::string &s)
{
    return QString::fromStdString(s);
}

QString quoteless(QString s)
{
    return s.remove(QLatin1Char('"'));
}

pj::AccountConfig toPjConfig(const AccountConfig &a)
{
    const QString tp = a.transport == QLatin1String("udp") ? QString()
                                                           : QStringLiteral(";transport=") + a.transport;
    pj::AccountConfig c;
    QString id = QStringLiteral("<sip:%1@%2>").arg(a.user, a.sipDomain());
    if (!a.displayName.isEmpty())
        id.prepend(QLatin1Char('"') + quoteless(a.displayName) + QStringLiteral("\" "));
    c.idUri = toStd(id);

    c.regConfig.registrarUri = toStd(QStringLiteral("sip:") + a.server + tp);
    c.regConfig.timeoutSec = unsigned(qMax(60, a.regExpiry));
    c.regConfig.retryIntervalSec = 30;
    c.regConfig.firstRetryIntervalSec = 5;

    const QString authUser = a.authUser.isEmpty() ? a.user : a.authUser;
    c.sipConfig.authCreds.push_back(pj::AuthCredInfo("digest", "*", toStd(authUser), 0, toStd(a.password)));
    if (!a.proxy.isEmpty()) {
        QString proxy = a.proxy;
        if (!proxy.startsWith(QLatin1String("sip:"), Qt::CaseInsensitive))
            proxy.prepend(QStringLiteral("sip:"));
        if (!proxy.contains(QLatin1String(";transport="), Qt::CaseInsensitive))
            proxy += tp;
        if (!proxy.contains(QLatin1String(";lr"), Qt::CaseInsensitive))
            proxy += QStringLiteral(";lr");
        c.sipConfig.proxies.push_back(toStd(proxy));
    }

    // Like MicroSIP's "allowRewrite": off by default. With a NAT between us and the PBX
    // (seen as another address) rewriting breaks calls on PBXs that handle NAT themselves.
    c.natConfig.viaRewriteUse = a.natRewrite;
    c.natConfig.sdpNatRewriteUse = a.natRewrite;
    c.natConfig.contactRewriteUse = a.natRewrite ? 2 : 0;
    c.natConfig.contactRewriteMethod = PJSUA_CONTACT_REWRITE_ALWAYS_UPDATE | PJSUA_CONTACT_REWRITE_UNREGISTER;

    c.mediaConfig.srtpUse = a.srtp ? PJMEDIA_SRTP_OPTIONAL : PJMEDIA_SRTP_DISABLED;
    c.mediaConfig.srtpSecureSignaling = 0;
    c.presConfig.publishEnabled = false;
    return c;
}

} // namespace

// ---------------------------------------------------------------------------

class KkAccount : public pj::Account {
public:
    explicit KkAccount(SipEngine *engine, QString id)
        : m_engine(engine), m_id(std::move(id)) {}

    void onRegState(pj::OnRegStateParam &prm) override
    {
        const int code = int(prm.code);
        if (code / 100 == 2) {
            state = prm.expiration > 0 ? RegState::Online : RegState::Disabled;
            text = state == RegState::Online ? QObject::tr("Online") : QObject::tr("Offline");
        } else {
            state = RegState::Failed;
            text = code > 0 ? QStringLiteral("%1 %2").arg(code).arg(fromStd(prm.reason))
                            : fromStd(prm.reason);
        }
        emit m_engine->regStateChanged(m_id);
    }

    void onIncomingCall(pj::OnIncomingCallParam &prm) override
    {
        m_engine->onIncoming(this, prm.callId);
    }

    QString id() const { return m_id; }

    RegState state = RegState::Registering;
    QString text = QObject::tr("Registering…");

private:
    SipEngine *m_engine;
    QString m_id;
};

class KkCall : public pj::Call {
public:
    KkCall(SipEngine *engine, pj::Account &acc, int callId = PJSUA_INVALID_ID)
        : pj::Call(acc, callId), m_engine(engine) {}

    void onCallState(pj::OnCallStateParam &) override { m_engine->onCallState(this); }
    void onCallMediaState(pj::OnCallMediaStateParam &) override { m_engine->onCallMedia(this); }

    void onCallTransferStatus(pj::OnCallTransferStatusParam &prm) override
    {
        if (prm.finalNotify && int(prm.statusCode) / 100 == 2) {
            prm.cont = false;
            pj::CallOpParam op(true);
            try { hangup(op); } catch (const pj::Error &) {}
        }
    }

    CallView view;
    bool mediaActive = false;

private:
    SipEngine *m_engine;
};

class KkBuddy : public pj::Buddy {
public:
    KkBuddy(SipEngine *engine, BlfTarget target)
        : m_engine(engine), target(std::move(target)) {}

    void onBuddyDlgEventState() override { m_engine->onBuddyDlgEvent(this); }

    BlfTarget target;
    BlfInfo info;

private:
    SipEngine *m_engine;
};

// ---------------------------------------------------------------------------

SipEngine::SipEngine(QObject *parent)
    : QObject(parent)
{
}

SipEngine::~SipEngine()
{
    shutdown();
}

bool SipEngine::start(QString *error)
{
    const Settings &s = Settings::instance();
    try {
        if (!s.debugLog)
            pj_log_set_level(0); // silence pjlib banner before libInit applies logConfig
        m_ep = std::make_unique<pj::Endpoint>();
        m_ep->libCreate();

        pj::EpConfig cfg;
        cfg.uaConfig.userAgent = "kk-sip/" KKSIP_VERSION;
        cfg.uaConfig.threadCnt = 0;
        cfg.uaConfig.mainThreadOnly = true;
        cfg.uaConfig.maxCalls = 8;
        cfg.medConfig.noVad = true;
        cfg.logConfig.consoleLevel = s.debugLog ? 4 : 0;
        cfg.logConfig.level = s.debugLog ? 5 : 1;
        cfg.logConfig.msgLogging = s.debugLog;
        if (s.debugLog) {
            const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
            QDir().mkpath(dir);
            cfg.logConfig.filename = toStd(dir + QStringLiteral("/pjsip.log"));
        }
        m_ep->libInit(cfg);

        pj::TransportConfig tcfg;
        tcfg.port = unsigned(s.sipPort);
        m_ep->transportCreate(PJSIP_TRANSPORT_UDP, tcfg);
        try {
            m_ep->transportCreate(PJSIP_TRANSPORT_TCP, tcfg);
        } catch (const pj::Error &e) {
            qWarning() << "TCP transport:" << fromStd(e.info());
        }
        try {
            pj::TransportConfig tls;
            tls.port = 0;
            m_ep->transportCreate(PJSIP_TRANSPORT_TLS, tls);
        } catch (const pj::Error &e) {
            qWarning() << "TLS transport:" << fromStd(e.info());
        }

        m_ep->libStart();

        m_tone = std::make_unique<pj::ToneGenerator>();
        m_tone->createToneGenerator();
    } catch (const pj::Error &e) {
        if (error)
            *error = fromStd(e.info());
        m_tone.reset();
        m_ep.reset();
        return false;
    }

    m_started = true;
    applyAudioDevices(s.captureDevice, s.playbackDevice);
    if (s.codecs.isEmpty()) {
        // PJSIP's own order puts Speex first; offer what office PBXs expect, MicroSIP-style.
        QList<CodecSetting> defaults;
        const QStringList preferred = {QStringLiteral("opus/48000/2"), QStringLiteral("G722/16000/1"),
                                       QStringLiteral("PCMA/8000/1"), QStringLiteral("PCMU/8000/1"),
                                       QStringLiteral("GSM/8000/1")};
        for (const QString &id : preferred)
            defaults.append({id, true});
        for (const CodecEntry &c : codecs())
            if (!preferred.contains(c.id))
                defaults.append({c.id, false});
        applyCodecs(defaults);
    } else {
        applyCodecs(s.codecs);
    }
    m_ringtoneFile = s.ringtoneFile;
    m_dnd = s.doNotDisturb;

    m_poll = new QTimer(this);
    m_poll->setInterval(10);
    connect(m_poll, &QTimer::timeout, this, [this] {
        if (!m_ep)
            return;
        // Drain whatever is pending without blocking the GUI.
        for (int i = 0; i < 16 && m_ep->libHandleEvents(0) > 0; ++i) {
        }
    });
    m_poll->start();
    return true;
}

void SipEngine::shutdown()
{
    if (!m_started)
        return;
    m_started = false;
    if (m_poll)
        m_poll->stop();

    stopTones();
    try {
        m_ep->hangupAllCalls();
    } catch (const pj::Error &) {
    }
    for (KkCall *c : std::as_const(m_calls))
        delete c;
    m_calls.clear();
    for (KkBuddy *b : std::as_const(m_buddies))
        delete b; // sends un-SUBSCRIBE
    m_buddies.clear();
    for (KkAccount *a : std::as_const(m_accounts))
        delete a; // sends un-REGISTER
    m_accounts.clear();

    m_ringPlayer.reset();
    m_tone.reset();
    try {
        m_ep->libDestroy();
    } catch (const pj::Error &) {
    }
    m_ep.reset();
}

// ---------------------------------------------------------------------------
// Accounts

void SipEngine::createAccount(const AccountConfig &cfg)
{
    auto *acc = new KkAccount(this, cfg.id);
    try {
        acc->create(toPjConfig(cfg));
    } catch (const pj::Error &e) {
        acc->state = RegState::Failed;
        acc->text = fromStd(e.reason);
    }
    m_accounts.insert(cfg.id, acc);
}

void SipEngine::removeAccount(const QString &id)
{
    deleteBuddiesOf(id); // buddies hold a reference to their account
    // Calls keep a reference to their account, end them first.
    for (KkCall *c : std::as_const(m_calls)) {
        if (c->view.accountId == id) {
            pj::CallOpParam prm(true);
            try { c->hangup(prm); } catch (const pj::Error &) {}
        }
    }
    if (std::none_of(m_calls.cbegin(), m_calls.cend(), [&](KkCall *c) { return c->view.accountId == id; })) {
        delete m_accounts.take(id); // sends un-REGISTER
    } else {
        // Still referenced by a dying call: unregister now, delete on the next apply.
        try { m_accounts.value(id)->setRegistration(false); } catch (const pj::Error &) {}
    }
}

void SipEngine::applyAccounts(const QList<AccountConfig> &accounts)
{
    if (!m_started)
        return;

    // Only enabled accounts become PJSIP accounts: PJSUA has a fixed number of slots.
    const QHash<QString, AccountConfig> previous = m_accountConfigs;
    m_accountConfigs.clear();
    m_accountErrors.clear();
    QSet<QString> live;
    for (const AccountConfig &cfg : accounts) {
        m_accountConfigs.insert(cfg.id, cfg);
        if (cfg.enabled)
            live.insert(cfg.id);
    }

    const QStringList existing = m_accounts.keys();
    for (const QString &id : existing)
        if (!live.contains(id))
            removeAccount(id);

    for (const AccountConfig &cfg : accounts) {
        if (cfg.enabled) {
            KkAccount *acc = m_accounts.value(cfg.id);
            if (!acc) {
                if (m_accounts.size() >= PJSUA_MAX_ACC)
                    m_accountErrors.insert(cfg.id, tr("Too many enabled accounts (limit %1)").arg(PJSUA_MAX_ACC));
                else
                    createAccount(cfg);
            } else if (!previous.value(cfg.id).sameSipConfig(cfg)) {
                try {
                    acc->modify(toPjConfig(cfg));
                    acc->state = RegState::Registering;
                    acc->text = tr("Registering…");
                    acc->setRegistration(true);
                } catch (const pj::Error &e) {
                    acc->state = RegState::Failed;
                    acc->text = fromStd(e.reason);
                }
            }
        }
        emit regStateChanged(cfg.id);
    }
    syncBuddies();
}

RegState SipEngine::regState(const QString &accountId) const
{
    if (KkAccount *acc = m_accounts.value(accountId))
        return acc->state;
    if (m_accountErrors.contains(accountId))
        return RegState::Failed;
    return RegState::Disabled;
}

QString SipEngine::regText(const QString &accountId) const
{
    if (KkAccount *acc = m_accounts.value(accountId))
        return acc->text;
    if (m_accountErrors.contains(accountId))
        return m_accountErrors.value(accountId);
    if (m_accountConfigs.contains(accountId))
        return tr("Disabled");
    return tr("Not configured");
}

const AccountConfig *SipEngine::accountConfig(const QString &accountId) const
{
    auto it = m_accountConfigs.constFind(accountId);
    return it == m_accountConfigs.cend() ? nullptr : &it.value();
}

QString SipEngine::accountIdFor(const KkAccount *acc) const
{
    return acc->id();
}

// ---------------------------------------------------------------------------
// BLF

void SipEngine::setBlfTargets(const QList<BlfTarget> &targets)
{
    m_blfTargets = targets;
    syncBuddies();
}

BlfInfo SipEngine::blf(qint64 contactId) const
{
    if (KkBuddy *b = m_buddies.value(contactId))
        return b->info;
    return {};
}

void SipEngine::deleteBuddiesOf(const QString &accountId)
{
    for (auto it = m_buddies.begin(); it != m_buddies.end();) {
        if (it.value()->target.accountId == accountId) {
            const qint64 id = it.key();
            delete it.value();
            it = m_buddies.erase(it);
            emit blfChanged(id);
        } else {
            ++it;
        }
    }
}

void SipEngine::syncBuddies()
{
    if (!m_started)
        return;
    QHash<qint64, BlfTarget> wanted;
    for (const BlfTarget &t : std::as_const(m_blfTargets))
        if (m_accounts.contains(t.accountId) && !t.number.isEmpty())
            wanted.insert(t.contactId, t);

    // Drop subscriptions that are gone or now point elsewhere.
    for (auto it = m_buddies.begin(); it != m_buddies.end();) {
        const auto w = wanted.constFind(it.key());
        const bool keep = w != wanted.cend() && w->accountId == it.value()->target.accountId
            && w->number == it.value()->target.number;
        if (keep) {
            ++it;
            continue;
        }
        const qint64 id = it.key();
        delete it.value();
        it = m_buddies.erase(it);
        emit blfChanged(id);
    }

    for (const BlfTarget &t : std::as_const(wanted)) {
        if (m_buddies.contains(t.contactId))
            continue;
        const AccountConfig *cfg = accountConfig(t.accountId);
        KkAccount *acc = m_accounts.value(t.accountId);
        if (!cfg || !acc)
            continue;
        auto *buddy = new KkBuddy(this, t);
        pj::BuddyConfig bc;
        bc.uri = toStd(SipUri::toTarget(t.number, *cfg));
        bc.subscribe = false;           // presence is not what PBX lamps use
        bc.subscribe_dlg_event = true;  // dialog event = BLF in Asterisk/FreePBX
        try {
            buddy->create(*acc, bc);
        } catch (const pj::Error &e) {
            qWarning() << "BLF subscribe" << t.number << fromStd(e.info());
            delete buddy;
            continue;
        }
        m_buddies.insert(t.contactId, buddy);
    }
}

void SipEngine::onBuddyDlgEvent(KkBuddy *buddy)
{
    pjsua_buddy_dlg_event_info di;
    if (pjsua_buddy_get_dlg_event_info(buddy->getId(), &di) != PJ_SUCCESS)
        return;
    const auto str = [](const pj_str_t &s) { return QString::fromUtf8(s.ptr, int(s.slen)); };

    qCDebug(lcBlf) << buddy->target.number << "sub" << di.sub_state_name << "state" << str(di.dialog_state)
                   << "dir" << str(di.dialog_direction) << "remote" << str(di.remote_identity);
    BlfInfo info;
    if (di.sub_state == PJSIP_EVSUB_STATE_ACTIVE || di.sub_state == PJSIP_EVSUB_STATE_PENDING) {
        const QString state = str(di.dialog_state).toLower();
        if (state.isEmpty() || state == QLatin1String("terminated"))
            info.state = BlfState::Idle;
        else if (state == QLatin1String("confirmed"))
            info.state = BlfState::Busy;
        else if (str(di.dialog_direction).toLower() == QLatin1String("initiator"))
            info.state = BlfState::Busy; // they are dialling out: nothing to pick up
        else // trying, proceeding, early on a call to them
            info.state = BlfState::Ringing;
        if (info.state != BlfState::Idle) {
            const QString display = str(di.remote_identity_display);
            const QString user = SipUri::parse(str(di.remote_identity)).user;
            if (display.isEmpty() || display == user)
                info.peer = user;
            else if (display.contains(user)) // "User 101" already names the extension
                info.peer = display;
            else
                info.peer = display + QLatin1Char(' ') + user;
        }
    }
    if (info.state == buddy->info.state && info.peer == buddy->info.peer)
        return;
    buddy->info = info;
    emit blfChanged(buddy->target.contactId);
}

// ---------------------------------------------------------------------------
// Calls

KkCall *SipEngine::findCall(int callId) const
{
    return m_calls.value(callId);
}

bool SipEngine::hasCall(int callId) const
{
    return m_calls.contains(callId);
}

QList<CallView> SipEngine::calls() const
{
    QList<CallView> out;
    for (KkCall *c : m_calls)
        out.append(c->view);
    std::sort(out.begin(), out.end(), [](const CallView &a, const CallView &b) { return a.id < b.id; });
    return out;
}

CallView SipEngine::call(int callId) const
{
    if (KkCall *c = findCall(callId))
        return c->view;
    return {};
}

int SipEngine::makeCall(const QString &accountId, const QString &input, QString *error)
{
    KkAccount *acc = m_accounts.value(accountId);
    const AccountConfig *cfg = accountConfig(accountId);
    if (!acc || !cfg) {
        if (error)
            *error = tr("No account selected");
        return -1;
    }
    const QString target = SipUri::toTarget(input, *cfg);
    if (target.isEmpty()) {
        if (error)
            *error = tr("Nothing to call");
        return -1;
    }

    holdOthers(-1);

    auto *call = new KkCall(this, *acc);
    call->view.accountId = accountId;
    call->view.number = SipUri::parse(target).user;
    call->view.incoming = false;
    call->view.state = CallView::Calling;
    call->view.startedAt = QDateTime::currentDateTime();

    pj::CallOpParam prm(true);
    prm.opt.audioCount = 1;
    prm.opt.videoCount = 0;
    try {
        call->makeCall(toStd(target), prm);
    } catch (const pj::Error &e) {
        if (error)
            *error = fromStd(e.reason);
        m_calls.remove(call->getId());
        delete call;
        return -1;
    }
    if (call->view.state == CallView::Ended) {
        // Failed synchronously; onCallState already scheduled the delete.
        if (error)
            *error = call->view.lastReason;
        return -1;
    }
    const int id = call->getId();
    call->view.id = id;
    m_calls.insert(id, call);
    emit callChanged(id);
    return id;
}

void SipEngine::onIncoming(KkAccount *acc, int pjCallId)
{
    auto *call = new KkCall(this, *acc, pjCallId);
    call->view.id = pjCallId;
    call->view.accountId = accountIdFor(acc);
    call->view.incoming = true;
    call->view.state = CallView::Incoming;
    call->view.startedAt = QDateTime::currentDateTime();
    try {
        const pj::CallInfo ci = call->getInfo();
        const SipUri::Parsed p = SipUri::parse(fromStd(ci.remoteUri));
        call->view.number = p.user;
        call->view.name = p.displayName;
    } catch (const pj::Error &) {
    }
    m_calls.insert(pjCallId, call);

    pj::CallOpParam prm;
    if (m_dnd) {
        prm.statusCode = PJSIP_SC_BUSY_HERE;
        try { call->hangup(prm); } catch (const pj::Error &) {}
        return;
    }
    prm.statusCode = PJSIP_SC_RINGING;
    try { call->answer(prm); } catch (const pj::Error &) {}

    emit incomingCall(pjCallId);
    updateTones();
}

void SipEngine::onCallState(KkCall *call)
{
    pj::CallInfo ci;
    try {
        ci = call->getInfo();
    } catch (const pj::Error &) {
        return;
    }
    const int id = ci.id;
    call->view.id = id;
    if (!m_calls.contains(id))
        m_calls.insert(id, call); // callback fired from inside makeCall()

    CallView &v = call->view;
    v.lastCode = int(ci.lastStatusCode);
    v.lastReason = fromStd(ci.lastReason);
    if (v.name.isEmpty())
        v.name = SipUri::parse(fromStd(ci.remoteUri)).displayName;

    switch (ci.state) {
    case PJSIP_INV_STATE_CALLING:
        v.state = CallView::Calling;
        break;
    case PJSIP_INV_STATE_INCOMING:
    case PJSIP_INV_STATE_EARLY:
        v.state = ci.role == PJSIP_ROLE_UAS ? CallView::Incoming : CallView::Ringing;
        break;
    case PJSIP_INV_STATE_CONNECTING:
        v.state = CallView::Connecting;
        break;
    case PJSIP_INV_STATE_CONFIRMED:
        v.state = CallView::Active;
        if (!v.answered) {
            v.answered = true;
            v.connectedAt = QDateTime::currentDateTime();
        }
        break;
    case PJSIP_INV_STATE_DISCONNECTED:
        v.state = CallView::Ended;
        break;
    default:
        break;
    }

    if (v.state == CallView::Ended) {
        const CallView finalView = v;
        m_calls.remove(id);
        updateTones();
        emit callEnded(finalView);
        // pjsua2 wants the Call object gone after DISCONNECTED; not from inside its own callback.
        QTimer::singleShot(0, this, [call] { delete call; });
        return;
    }

    updateTones();
    emit callChanged(id);
}

void SipEngine::onCallMedia(KkCall *call)
{
    connectCallAudio(call);
    updateTones();
    emit callChanged(call->view.id);
}

void SipEngine::connectCallAudio(KkCall *call)
{
    pj::CallInfo ci;
    try {
        ci = call->getInfo();
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
            pj::AudioMedia am = call->getAudioMedia(int(i));
            pj::AudDevManager &mgr = m_ep->audDevManager();
            am.startTransmit(mgr.getPlaybackDevMedia());
            if (call->view.muted)
                mgr.getCaptureDevMedia().stopTransmit(am);
            else
                mgr.getCaptureDevMedia().startTransmit(am);
            active = true;
        } catch (const pj::Error &e) {
            qWarning() << "audio connect:" << fromStd(e.info());
        }
    }
    call->mediaActive = active;
    call->view.onHold = localHold;
}

void SipEngine::holdOthers(int exceptCallId)
{
    for (KkCall *c : std::as_const(m_calls)) {
        if (c->view.id == exceptCallId || c->view.state != CallView::Active || c->view.onHold)
            continue;
        setHold(c->view.id, true);
    }
}

void SipEngine::answer(int callId)
{
    KkCall *call = findCall(callId);
    if (!call || call->view.state != CallView::Incoming)
        return;
    holdOthers(callId);
    pj::CallOpParam prm;
    prm.statusCode = PJSIP_SC_OK;
    prm.opt.audioCount = 1;
    prm.opt.videoCount = 0;
    try {
        call->answer(prm);
    } catch (const pj::Error &e) {
        qWarning() << "answer:" << fromStd(e.info());
    }
    call->view.state = CallView::Connecting;
    updateTones();
    emit callChanged(callId);
}

void SipEngine::hangup(int callId)
{
    KkCall *call = findCall(callId);
    if (!call)
        return;
    pj::CallOpParam prm(true);
    if (call->view.state == CallView::Incoming) {
        prm.statusCode = PJSIP_SC_DECLINE;
        call->view.declined = true;
    }
    try {
        call->hangup(prm);
    } catch (const pj::Error &e) {
        qWarning() << "hangup:" << fromStd(e.info());
    }
}

void SipEngine::hangupAll()
{
    const QList<int> ids = m_calls.keys();
    for (int id : ids)
        hangup(id);
}

void SipEngine::setHold(int callId, bool hold)
{
    KkCall *call = findCall(callId);
    if (!call || call->view.state != CallView::Active)
        return;
    pj::CallOpParam prm(true);
    try {
        if (hold) {
            call->setHold(prm);
        } else {
            holdOthers(callId);
            prm.opt.flag = PJSUA_CALL_UNHOLD;
            call->reinvite(prm);
        }
        // onHold follows the negotiated media state (connectCallAudio), not the request.
    } catch (const pj::Error &e) {
        qWarning() << "hold:" << fromStd(e.info());
    }
    emit callChanged(callId);
}

void SipEngine::setMute(int callId, bool mute)
{
    KkCall *call = findCall(callId);
    if (!call)
        return;
    call->view.muted = mute;
    connectCallAudio(call);
    emit callChanged(callId);
}

void SipEngine::sendDtmf(int callId, const QString &digits)
{
    KkCall *call = findCall(callId);
    if (!call || call->view.state != CallView::Active)
        return;
    try {
        call->dialDtmf(toStd(digits));
    } catch (const pj::Error &e) {
        qWarning() << "dtmf:" << fromStd(e.info());
    }
}

bool SipEngine::transfer(int callId, const QString &input, QString *error)
{
    KkCall *call = findCall(callId);
    const AccountConfig *cfg = call ? accountConfig(call->view.accountId) : nullptr;
    if (!call || !cfg)
        return false;
    const QString target = SipUri::toTarget(input, *cfg);
    if (target.isEmpty())
        return false;
    pj::CallOpParam prm(true);
    try {
        call->xfer(toStd(target), prm);
    } catch (const pj::Error &e) {
        if (error)
            *error = fromStd(e.reason);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Ring / ringback tones

void SipEngine::updateTones()
{
    Tone want = Tone::None;
    for (KkCall *c : std::as_const(m_calls)) {
        if (c->view.state == CallView::Incoming && !m_dnd) {
            want = Tone::Ring;
            break;
        }
        // 180 without early media: the far end rings but sends no audio, play our own.
        if (c->view.state == CallView::Ringing && c->view.lastCode == 180 && !c->mediaActive)
            want = Tone::Ringback;
    }
    if (want == m_toneState)
        return;
    stopTones();
    if (want == Tone::Ring)
        startRing();
    else if (want == Tone::Ringback)
        startRingback();
    m_toneState = want;
}

void SipEngine::startRing()
{
    try {
        pj::AudioMedia &out = m_ep->audDevManager().getPlaybackDevMedia();
        if (!m_ringtoneFile.isEmpty() && QFile::exists(m_ringtoneFile)) {
            m_ringPlayer = std::make_unique<pj::AudioMediaPlayer>();
            m_ringPlayer->createPlayer(toStd(m_ringtoneFile), 0); // loops
            m_ringPlayer->startTransmit(out);
            return;
        }
        // Double ring: 400+450 Hz, 0.4 s on / 0.2 s off / 0.4 s on / 2 s off.
        pj::ToneDescVector tones;
        pj::ToneDesc t;
        t.freq1 = 400;
        t.freq2 = 450;
        t.on_msec = 400;
        t.off_msec = 200;
        tones.push_back(t);
        t.off_msec = 2000;
        tones.push_back(t);
        m_tone->play(tones, true);
        m_tone->startTransmit(out);
    } catch (const pj::Error &e) {
        qWarning() << "ring:" << fromStd(e.info());
    }
}

void SipEngine::startRingback()
{
    try {
        pj::ToneDescVector tones;
        pj::ToneDesc t;
        t.freq1 = 425;
        t.freq2 = 0;
        t.on_msec = 1000;
        t.off_msec = 4000;
        tones.push_back(t);
        m_tone->play(tones, true);
        m_tone->startTransmit(m_ep->audDevManager().getPlaybackDevMedia());
    } catch (const pj::Error &e) {
        qWarning() << "ringback:" << fromStd(e.info());
    }
}

void SipEngine::stopTones()
{
    m_toneState = Tone::None;
    if (!m_ep)
        return;
    try {
        pj::AudioMedia &out = m_ep->audDevManager().getPlaybackDevMedia();
        if (m_tone && m_tone->isBusy()) {
            m_tone->stop();
        }
        if (m_tone)
            m_tone->stopTransmit(out);
        if (m_ringPlayer) {
            m_ringPlayer->stopTransmit(out);
            m_ringPlayer.reset();
        }
    } catch (const pj::Error &) {
    }
}

// ---------------------------------------------------------------------------
// Audio devices and codecs

QList<AudioDevice> SipEngine::audioDevices() const
{
    QList<AudioDevice> out;
    if (!m_ep)
        return out;
    try {
        const pj::AudioDevInfoVector2 devs = m_ep->audDevManager().enumDev2();
        // ALSA lists every plugin and channel layout; under PipeWire/Pulse only these make sense.
        static const QRegularExpression junk(QStringLiteral(
            "^(lavrate|samplerate|speexrate|speex|jack|oss|upmix|vdownmix|null)$"
            "|^(usbstream|surround\\d+|iec958|dmix|dsnoop|plughw|hw):"));
        for (const pj::AudioDevInfo &d : devs) {
            if (junk.match(fromStd(d.name)).hasMatch())
                continue;
            AudioDevice a;
            a.name = fromStd(d.name);
            a.key = fromStd(d.driver) + QLatin1Char('|') + a.name;
            a.input = d.inputCount > 0;
            a.output = d.outputCount > 0;
            out.append(a);
        }
    } catch (const pj::Error &) {
    }
    return out;
}

void SipEngine::applyAudioDevices(const QString &captureKey, const QString &playbackKey)
{
    if (!m_ep)
        return;
    pj::AudDevManager &mgr = m_ep->audDevManager();
    if (captureKey == QLatin1String("null") || playbackKey == QLatin1String("null")) {
        // No sound card at all (tests, headless): PJSIP's clock drives the conference bridge.
        try { mgr.setNullDev(); } catch (const pj::Error &) {}
        return;
    }
    const QList<AudioDevice> devs = audioDevices();

    // Explicit choice first, otherwise the sound server's ALSA plugin: on a
    // PipeWire/Pulse desktop that follows the system default device.
    auto pick = [&](const QString &key, bool input) -> int {
        QStringList candidates;
        if (!key.isEmpty())
            candidates << key;
        candidates << QStringLiteral("ALSA|pipewire") << QStringLiteral("ALSA|pulse")
                   << QStringLiteral("ALSA|default");
        for (const QString &k : std::as_const(candidates)) {
            for (const AudioDevice &d : devs) {
                if (d.key == k && (input ? d.input : d.output)) {
                    const int sep = k.indexOf(QLatin1Char('|'));
                    try {
                        return mgr.lookupDev(toStd(k.left(sep)), toStd(k.mid(sep + 1)));
                    } catch (const pj::Error &) {
                    }
                }
            }
        }
        return input ? PJMEDIA_AUD_DEFAULT_CAPTURE_DEV : PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;
    };

    try {
        mgr.setCaptureDev(pick(captureKey, true));
        mgr.setPlaybackDev(pick(playbackKey, false));
    } catch (const pj::Error &e) {
        qWarning() << "audio devices:" << fromStd(e.info());
    }
}

QList<CodecEntry> SipEngine::codecs() const
{
    QList<CodecEntry> out;
    if (!m_ep)
        return out;
    try {
        const pj::CodecInfoVector2 list = m_ep->codecEnum2();
        for (const pj::CodecInfo &c : list)
            out.append({fromStd(c.codecId), int(c.priority)});
    } catch (const pj::Error &) {
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const CodecEntry &a, const CodecEntry &b) { return a.priority > b.priority; });
    return out;
}

void SipEngine::applyCodecs(const QList<CodecSetting> &codecs)
{
    if (!m_ep || codecs.isEmpty())
        return;
    int prio = 254;
    for (const CodecSetting &c : codecs) {
        try {
            m_ep->codecSetPriority(toStd(c.id), pj_uint8_t(c.enabled ? qMax(1, prio) : 0));
        } catch (const pj::Error &) {
            // codec vanished (e.g. built without it), ignore
        }
        if (c.enabled)
            --prio;
    }
}
