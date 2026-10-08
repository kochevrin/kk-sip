#pragma once

#include "core/Settings.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <memory>

class QTimer;

namespace pj {
class Endpoint;
class ToneGenerator;
class AudioMediaPlayer;
} // namespace pj

class KkAccount;
class KkBuddy;
class KkCall;

enum class RegState { Disabled, Registering, Online, Failed };

struct CallView {
    enum State { Calling, Incoming, Ringing, Connecting, Active, Ended };

    int id = -1;
    QString accountId;
    QString number;      // user part of the remote URI
    QString name;        // display name from SIP, if any
    bool incoming = false;
    State state = Calling;
    bool onHold = false;
    bool muted = false;
    bool answered = false;
    bool declined = false; // incoming call rejected by us
    QDateTime startedAt;
    QDateTime connectedAt;
    int lastCode = 0;
    QString lastReason;
};

// Busy lamp field: dialog-event (RFC 4235) subscription to a colleague's extension.
enum class BlfState { Unknown, Idle, Ringing, Busy };

struct BlfTarget {
    qint64 contactId = 0;
    QString accountId;
    QString number;
};

struct BlfInfo {
    BlfState state = BlfState::Unknown;
    QString peer; // who they are talking to / who is calling them, when the PBX says
};

struct AudioDevice {
    QString key;   // "driver|name", stored in settings
    QString name;
    bool input = false;
    bool output = false;
};

struct CodecEntry {
    QString id;
    int priority = 0;
};

// Qt facade over PJSUA2. Everything runs on the GUI thread: PJSIP is polled
// from a QTimer (threadCnt = 0, mainThreadOnly), so callbacks are safe to
// touch Qt objects directly.
class SipEngine : public QObject {
    Q_OBJECT
public:
    explicit SipEngine(QObject *parent = nullptr);
    ~SipEngine() override;

    bool start(QString *error);
    void shutdown();

    void applyAccounts(const QList<AccountConfig> &accounts);
    RegState regState(const QString &accountId) const;
    QString regText(const QString &accountId) const;

    int makeCall(const QString &accountId, const QString &input, QString *error);
    void answer(int callId);
    void hangup(int callId);
    void setHold(int callId, bool hold);
    void setMute(int callId, bool mute);
    void sendDtmf(int callId, const QString &digits);
    bool transfer(int callId, const QString &input, QString *error);
    void hangupAll();

    QList<CallView> calls() const;
    CallView call(int callId) const;
    bool hasCall(int callId) const;

    QList<AudioDevice> audioDevices() const;
    void applyAudioDevices(const QString &captureKey, const QString &playbackKey);

    QList<CodecEntry> codecs() const;
    void applyCodecs(const QList<CodecSetting> &codecs);

    void setDoNotDisturb(bool on) { m_dnd = on; }
    bool doNotDisturb() const { return m_dnd; }

    void setRingtone(const QString &wavFile) { m_ringtoneFile = wavFile; }

    // Replaces the set of watched extensions; unchanged ones keep their subscription.
    void setBlfTargets(const QList<BlfTarget> &targets);
    BlfInfo blf(qint64 contactId) const;

signals:
    void regStateChanged(const QString &accountId);
    void incomingCall(int callId);
    void callChanged(int callId);
    void callEnded(const CallView &call);
    void blfChanged(qint64 contactId);

private:
    friend class KkAccount;
    friend class KkCall;
    friend class KkBuddy;

    void onIncoming(KkAccount *acc, int pjCallId);
    void onCallState(KkCall *call);
    void onCallMedia(KkCall *call);
    void updateTones();
    void startRing();
    void startRingback();
    void stopTones();
    void connectCallAudio(KkCall *call);
    void holdOthers(int exceptCallId);
    KkCall *findCall(int callId) const;
    QString accountIdFor(const KkAccount *acc) const;
    const AccountConfig *accountConfig(const QString &accountId) const;
    void createAccount(const AccountConfig &cfg);
    void removeAccount(const QString &id);
    void syncBuddies();
    void deleteBuddiesOf(const QString &accountId);
    void onBuddyDlgEvent(KkBuddy *buddy);

    std::unique_ptr<pj::Endpoint> m_ep;
    QTimer *m_poll = nullptr;
    QHash<QString, KkAccount *> m_accounts;
    QHash<QString, AccountConfig> m_accountConfigs;
    QHash<QString, QString> m_accountErrors;
    QHash<int, KkCall *> m_calls;
    QHash<qint64, KkBuddy *> m_buddies; // by contact id
    QList<BlfTarget> m_blfTargets;
    std::unique_ptr<pj::ToneGenerator> m_tone;
    std::unique_ptr<pj::AudioMediaPlayer> m_ringPlayer;
    enum class Tone { None, Ring, Ringback } m_toneState = Tone::None;
    QString m_ringtoneFile;
    bool m_dnd = false;
    bool m_started = false;
};
