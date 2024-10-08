#include <unistd.h>

#include <QStringList>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsocket.h"
#include "libmythbase/programinfo.h"

#include "recorders/signalmonitor.h"
#include "remoteencoder.h"

#define LOC QString("RemoteEncoder(%1): ").arg(m_recordernum)

static constexpr std::chrono::milliseconds MAX_SIZE_CHECK { 500ms };

RemoteEncoder::~RemoteEncoder()
{
    if (m_controlSock)
    {
        m_controlSock->DecrRef();
        m_controlSock = nullptr;
    }
}

bool RemoteEncoder::Setup(void)
{
    if (!m_controlSock)
    {
        LOG(VB_NETWORK, LOG_DEBUG, "RemoteEncoder::Setup(): Connecting...");

        QString ann = QString("ANN Playback %1 %2")
            .arg(gCoreContext->GetHostName()).arg(static_cast<int>(false));

        m_controlSock = gCoreContext->ConnectCommandSocket(
            m_remotehost, m_remoteport, ann);

        if (m_controlSock)
        {
            LOG(VB_NETWORK, LOG_DEBUG, "RemoteEncoder::Setup(): Connected");
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                "RemoteEncoder::Setup(): Failed to connect to backend");
        }
    }
    else
    {
        LOG(VB_NETWORK, LOG_DEBUG, "RemoteEncoder::Setup(): Already connected");
    }
    return m_controlSock;
}

bool RemoteEncoder::IsValidRecorder(void) const
{
    return (m_recordernum >= 0);
}

int RemoteEncoder::GetRecorderNumber(void) const
{
    return m_recordernum;
}

bool RemoteEncoder::SendReceiveStringList(
    QStringList &strlist, uint min_reply_length)
{
    QMutexLocker locker(&m_lock);
    if (!m_controlSock)
        Setup();

    m_backendError = false;

    if (!m_controlSock)
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoteEncoder::SendReceiveStringList(): "
                                 "Failed to reconnect with backend.");
        m_backendError = true;
        return false;
    }

    if (!m_controlSock->WriteStringList(strlist))
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoteEncoder::SendReceiveStringList(): "
                                 "Failed to write data.");
        m_backendError = true;
    }

    if (!m_backendError &&
        !m_controlSock->ReadStringList(strlist, MythSocket::kShortTimeout))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "RemoteEncoder::SendReceiveStringList(): No response.");
        m_backendError = true;
    }

    if (!m_backendError &&
        min_reply_length && ((uint)strlist.size() < min_reply_length))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "RemoteEncoder::SendReceiveStringList(): Response too short");
        m_backendError = true;
    }

    if (m_backendError)
    {
        m_controlSock->DecrRef();
        m_controlSock = nullptr;
        return false;
    }

    return true;
}

bool RemoteEncoder::IsRecording(bool *ok)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "IS_RECORDING";

    bool ret = SendReceiveStringList(strlist, 1);
    if (!ret)
    {
        if (ok)
            *ok = false;

        return false;
    }

    if (ok)
        *ok = true;

    return strlist[0].toInt() != 0;
}

ProgramInfo *RemoteEncoder::GetRecording(void)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "GET_RECORDING";

    if (SendReceiveStringList(strlist))
    {
        auto *proginfo = new ProgramInfo(strlist);
        if (proginfo->GetChanID())
            return proginfo;
        delete proginfo;
    }

    return nullptr;
}

/** \fn RemoteEncoder::GetFrameRate(void)
 *  \brief Returns recordering frame rate set by nvr.
 *  \sa TVRec::GetFramerate(void), EncoderLink::GetFramerate(void),
 *      RecorderBase::GetFrameRate(void)
 *  \return Frames per second if query succeeds -1 otherwise.
 */
float RemoteEncoder::GetFrameRate(void)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum));
    strlist << "GET_FRAMERATE";

    bool ok = false;
    float retval = 30.0F;

    if (SendReceiveStringList(strlist, 1))
    {
        retval = strlist[0].toFloat(&ok);

        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("GetFrameRate() failed to parse response '%1'")
                    .arg(strlist[0]));
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "GetFrameRate(): SendReceiveStringList() failed");
    }

    return (ok) ? retval : 30.0F;
}

/** \fn RemoteEncoder::GetFramesWritten(void)
 *  \brief Returns number of frames written to disk by TVRec's RecorderBase
 *         instance.
 *
 *  \sa TVRec::GetFramesWritten(void), EncoderLink::GetFramesWritten(void)
 *  \return Number of frames if query succeeds, return last known value otherwise.
 */
long long RemoteEncoder::GetFramesWritten(void)
{
    if (m_lastTimeCheck.isRunning() && m_lastTimeCheck.elapsed() < MAX_SIZE_CHECK)
    {
        return m_cachedFramesWritten;
    }

    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum));
    strlist << "GET_FRAMES_WRITTEN";

    if (!SendReceiveStringList(strlist, 1))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GetFramesWritten() -- network error");
    }
    else
    {
        m_cachedFramesWritten = strlist[0].toLongLong();
        m_lastTimeCheck.restart();
    }

    return m_cachedFramesWritten;
}

/** \fn RemoteEncoder::GetFilePosition(void)
 *  \brief Returns total number of bytes written by TVRec's RingBuffer.
 *
 *  \sa TVRec::GetFilePosition(void), EncoderLink::GetFilePosition(void)
 *  \return Bytes written if query succeeds, -1 otherwise.
 */
long long RemoteEncoder::GetFilePosition(void)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum));
    strlist << "GET_FILE_POSITION";

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toLongLong();

    return -1;
}

/**
 *   Returns the maximum bits per second this recorder can produce.
 *  \sa TVRec::GetMaxBitrate(void), EncoderLink::GetMaxBitrate(void)
 */
long long RemoteEncoder::GetMaxBitrate(void)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum));
    strlist << "GET_MAX_BITRATE";

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toLongLong();

    return 20200000LL; // Peek bit rate for HD-PVR
}

/** \fn RemoteEncoder::GetKeyframePosition(uint64_t)
 *  \brief Returns byte position in RingBuffer of a keyframe.
 *
 *  \sa TVRec::GetKeyframePosition(uint64_t),
 *      EncoderLink::GetKeyframePosition(uint64_t)
 *  \return Byte position of keyframe if query succeeds, -1 otherwise.
 */
int64_t RemoteEncoder::GetKeyframePosition(uint64_t desired)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "GET_KEYFRAME_POS";
    strlist << QString::number(desired);

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toLongLong();

    return -1;
}

void RemoteEncoder::FillPositionMap(int64_t start, int64_t end,
                                    frm_pos_map_t &positionMap)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum));
    strlist << "FILL_POSITION_MAP";
    strlist << QString::number(start);
    strlist << QString::number(end);

    if (!SendReceiveStringList(strlist))
        return;

    for (auto it = strlist.cbegin(); it != strlist.cend(); ++it)
    {
        bool ok = false;
        uint64_t index = (*it).toLongLong(&ok);
        if (++it == strlist.cend() || !ok)
            break;

        uint64_t pos = (*it).toLongLong(&ok);
        if (!ok)
            break;

        positionMap[index] = pos;
    }
}

void RemoteEncoder::FillDurationMap(int64_t start, int64_t end,
                                    frm_pos_map_t &durationMap)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum));
    strlist << "FILL_DURATION_MAP";
    strlist << QString::number(start);
    strlist << QString::number(end);

    if (!SendReceiveStringList(strlist))
        return;

    for (auto it = strlist.cbegin(); it != strlist.cend(); ++it)
    {
        bool ok = false;
        uint64_t index = (*it).toLongLong(&ok);
        if (++it == strlist.cend() || !ok)
            break;

        uint64_t pos = (*it).toLongLong(&ok);
        if (!ok)
            break;

        durationMap[index] = pos;
    }
}

void RemoteEncoder::CancelNextRecording(bool cancel)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum));
    strlist << "CANCEL_NEXT_RECORDING";
    strlist << QString::number((cancel) ? 1 : 0);

    SendReceiveStringList(strlist);
}

void RemoteEncoder::FrontendReady(void)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum));
    strlist << "FRONTEND_READY";

    SendReceiveStringList(strlist);
}

/** \fn RemoteEncoder::StopPlaying(void)
 *  \brief Tells TVRec to stop streaming a recording to the frontend.
 *  \sa TVRec::StopPlaying(void), EncoderLink::StopPlaying(void)
 */
void RemoteEncoder::StopPlaying(void)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "STOP_PLAYING";

    SendReceiveStringList(strlist);
}

/** \fn RemoteEncoder::SpawnLiveTV(QString,bool,QString)
 *  \brief Tells TVRec to Spawn a "Live TV" recorder.
 *  \sa TVRec::SpawnLiveTV(LiveTVChain*,bool,QString),
 *      EncoderLink::SpawnLiveTV(LiveTVChain*,bool,QString)
 */
void RemoteEncoder::SpawnLiveTV(const QString& chainId, bool pip, const QString& startchan)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum));
    strlist << "SPAWN_LIVETV";
    strlist << chainId;
    strlist << QString::number((int)pip);
    strlist << startchan;

    SendReceiveStringList(strlist);
}

/**
 *  \brief Tells TVRec to stop a "Live TV" recorder.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::StopLiveTV(void), EncoderLink::StopLiveTV(void)
 */
void RemoteEncoder::StopLiveTV(void)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "STOP_LIVETV";

    SendReceiveStringList(strlist);
}

/** \fn RemoteEncoder::PauseRecorder(void)
 *  \brief Tells TVRec to pause a recorder, used for channel and input changes.
 *  \sa TVRec::PauseRecorder(void), EncoderLink::PauseRecorder(void),
 *      RecorderBase::Pause(void)
 */
void RemoteEncoder::PauseRecorder(void)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "PAUSE";

    if (SendReceiveStringList(strlist))
        m_lastinput = "";
}

void RemoteEncoder::FinishRecording(void)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "FINISH_RECORDING";

    SendReceiveStringList(strlist);
}

void RemoteEncoder::SetLiveRecording(bool recording)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "SET_LIVE_RECORDING";
    strlist << QString::number(static_cast<int>(recording));

    SendReceiveStringList(strlist);
}

QString RemoteEncoder::GetInput(void)
{
    if (!m_lastinput.isEmpty())
        return m_lastinput;

    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "GET_INPUT";

    if (SendReceiveStringList(strlist, 1))
    {
        m_lastinput = strlist[0];
        return m_lastinput;
    }

    return "Error";
}

QString RemoteEncoder::SetInput(const QString& input)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "SET_INPUT";
    strlist << input;

    if (SendReceiveStringList(strlist, 1))
    {
        m_lastchannel = "";
        m_lastinput = "";
        return strlist[0];
    }

    return (m_lastinput.isEmpty()) ? "Error" : m_lastinput;
}

void RemoteEncoder::ToggleChannelFavorite(const QString& changroupname)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "TOGGLE_CHANNEL_FAVORITE";
    strlist << changroupname;

    SendReceiveStringList(strlist);
}

void RemoteEncoder::ChangeChannel(int channeldirection)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "CHANGE_CHANNEL";
    strlist << QString::number(channeldirection);

    if (!SendReceiveStringList(strlist))
        return;

    m_lastchannel = "";
    m_lastinput = "";
}

void RemoteEncoder::SetChannel(const QString& channel)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "SET_CHANNEL";
    strlist << channel;

    if (!SendReceiveStringList(strlist))
        return;

    m_lastchannel = "";
    m_lastinput = "";
}

/**
 *  \brief Sets the signal monitoring rate.
 *
 *  This will actually call SetupSignalMonitor() and
 *  TeardownSignalMonitor(bool) as needed, so it can
 *  be used directly, without worrying about the
 *  SignalMonitor instance.
 *
 *  \sa TVRec::SetSignalMonitoringRate(milliseconds,int),
 *      EncoderLink::SetSignalMonitoringRate(milliseconds,int)
 *  \param rate           The update rate to use in milliseconds,
 *                        use 0 to disable.
 *  \param notifyFrontend If true, SIGNAL messages will be sent to
 *                        the frontend using this recorder.
 *  \return Previous update rate
 */
std::chrono::milliseconds RemoteEncoder::SetSignalMonitoringRate(std::chrono::milliseconds rate, int notifyFrontend)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "SET_SIGNAL_MONITORING_RATE";
    strlist << QString::number(rate.count());
    strlist << QString::number((int)notifyFrontend);

    if (SendReceiveStringList(strlist, 1))
        return std::chrono::milliseconds(strlist[0].toInt());

    return 0ms;
}

uint RemoteEncoder::GetSignalLockTimeout(const QString& input)
{
    QMutexLocker locker(&m_lock);

    QMap<QString,uint>::const_iterator it = m_cachedTimeout.constFind(input);
    if (it != m_cachedTimeout.constEnd())
        return *it;

    uint cardid  = m_recordernum;
    uint timeout = 0xffffffff;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT channel_timeout, cardtype "
        "FROM capturecard "
        "WHERE capturecard.inputname = :INNAME AND "
        "      capturecard.cardid    = :CARDID");
    query.bindValue(":INNAME", input);
    query.bindValue(":CARDID", cardid);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("Getting timeout", query);
    else if (query.next() &&
             SignalMonitor::IsRequired(query.value(1).toString()))
        timeout = std::max(query.value(0).toInt(), 500);

#if 0
    LOG(VB_PLAYBACK, LOG_DEBUG, "RemoteEncoder: " +
        QString("GetSignalLockTimeout(%1): Set lock timeout to %2 ms")
            .arg(cardid).arg(timeout));
#endif
    m_cachedTimeout[input] = timeout;
    return timeout;
}


int RemoteEncoder::GetPictureAttribute(PictureAttribute attr)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );

    if (kPictureAttribute_Contrast == attr)
        strlist << "GET_CONTRAST";
    else if (kPictureAttribute_Brightness == attr)
        strlist << "GET_BRIGHTNESS";
    else if (kPictureAttribute_Colour == attr)
        strlist << "GET_COLOUR";
    else if (kPictureAttribute_Hue == attr)
        strlist << "GET_HUE";
    else
        return -1;

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toInt();

    return -1;
}

/**
 *  \brief Changes brightness/contrast/colour/hue of a recording.
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return contrast if it succeeds, -1 otherwise.
 */
int RemoteEncoder::ChangePictureAttribute(
    PictureAdjustType type, PictureAttribute attr, bool up)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );

    if (kPictureAttribute_Contrast == attr)
        strlist << "CHANGE_CONTRAST";
    else if (kPictureAttribute_Brightness == attr)
        strlist << "CHANGE_BRIGHTNESS";
    else if (kPictureAttribute_Colour == attr)
        strlist << "CHANGE_COLOUR";
    else if (kPictureAttribute_Hue == attr)
        strlist << "CHANGE_HUE";
    else
        return -1;

    strlist << QString::number(type);
    strlist << QString::number((int)up);

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toInt();

    return -1;
}

void RemoteEncoder::ChangeDeinterlacer(int deint_mode)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "CHANGE_DEINTERLACER";
    strlist << QString::number(deint_mode);

    SendReceiveStringList(strlist);
}

/** \fn RemoteEncoder::CheckChannel(QString)
 *  \brief Checks if named channel exists on current tuner.
 *
 *  \param channel Channel to verify against current tuner.
 *  \return true if it succeeds, false otherwise.
 *  \sa TVRec::CheckChannel(QString),
 *      EncoderLink::CheckChannel(const QString&),
 *      ShouldSwitchToAnotherCard(QString)
 */
bool RemoteEncoder::CheckChannel(const QString& channel)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "CHECK_CHANNEL";
    strlist << channel;

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toInt() != 0;

    return false;
}

/** \fn RemoteEncoder::ShouldSwitchToAnotherCard(QString)
 *  \brief Checks if named channel exists on current tuner, or
 *         another tuner.
 *         <b>This only works on local recorders.</b>
 *  \param channelid channel to verify against tuners.
 *  \return true if the channel on another tuner and not current tuner,
 *          false otherwise.
 *  \sa CheckChannel(const QString&)
 */
bool RemoteEncoder::ShouldSwitchToAnotherCard(const QString& channelid)
{
    // this function returns true if the channelid is not a valid
    // channel on the current recorder. It queries to server in order
    // to determine this.
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "SHOULD_SWITCH_CARD";
    strlist << channelid;

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toInt() != 0;

    return false;
}

/** \fn RemoteEncoder::CheckChannelPrefix(const QString&,uint&,bool&,QString&)
 *  \brief Checks a prefix against the channels in the DB.
 *
 *  \sa TVRec::CheckChannelPrefix(const QString&,uint&,bool&,QString&)
 *      for details.
 */
bool RemoteEncoder::CheckChannelPrefix(
    const QString &prefix,
    uint          &complete_valid_channel_on_rec,
    bool          &is_extra_char_useful,
    QString       &needed_spacer)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "CHECK_CHANNEL_PREFIX";
    strlist << prefix;

    if (!SendReceiveStringList(strlist, 4))
        return false;

    complete_valid_channel_on_rec = strlist[1].toInt();
    is_extra_char_useful = (strlist[2].toInt() != 0);
    needed_spacer = (strlist[3] == "X") ? "" : strlist[3];

    return strlist[0].toInt() != 0;
}

static QString cleanup(const QString &str)
{
    if (str == " ")
        return "";
    return str;
}

static QString make_safe(const QString &str)
{
    if (str.isEmpty())
        return " ";
    return str;
}

/** \fn RemoteEncoder::GetNextProgram(int,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&)
 *  \brief Returns information about the program that would be seen if we changed
 *         the channel using ChangeChannel(int) with "direction".
 *  \sa TVRec::GetNextProgram(int,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&),
 *      EncoderLink::GetNextProgram(int,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&)
 */
void RemoteEncoder::GetNextProgram(int direction,
                                   QString &title, QString &subtitle,
                                   QString &desc, QString &category,
                                   QString &starttime, QString &endtime,
                                   QString &callsign, QString &iconpath,
                                   QString &channelname, QString &chanid,
                                   QString &seriesid, QString &programid)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum) );
    strlist << "GET_NEXT_PROGRAM_INFO";
    strlist << channelname;
    strlist << chanid;
    strlist << QString::number(direction);
    strlist << starttime;

    if (!SendReceiveStringList(strlist, 12))
        return;

    title       = cleanup(strlist[0]);
    subtitle    = cleanup(strlist[1]);
    desc        = cleanup(strlist[2]);
    category    = cleanup(strlist[3]);
    starttime   = cleanup(strlist[4]);
    endtime     = cleanup(strlist[5]);
    callsign    = cleanup(strlist[6]);
    iconpath    = cleanup(strlist[7]);
    channelname = cleanup(strlist[8]);
    chanid      = cleanup(strlist[9]);
    seriesid    = cleanup(strlist[10]);
    programid   = cleanup(strlist[11]);
}

void RemoteEncoder::GetChannelInfo(InfoMap &infoMap, uint chanid)
{
    QStringList strlist( QString("QUERY_RECORDER %1").arg(m_recordernum));
    strlist << "GET_CHANNEL_INFO";
    strlist << QString::number(chanid);

    if (!SendReceiveStringList(strlist, 6))
        return;

    infoMap["chanid"]   = cleanup(strlist[0]);
    infoMap["sourceid"] = cleanup(strlist[1]);
    infoMap["callsign"] = cleanup(strlist[2]);
    infoMap["channum"]  = cleanup(strlist[3]);
    infoMap["channame"] = cleanup(strlist[4]);
    infoMap["XMLTV"]    = cleanup(strlist[5]);

    infoMap["oldchannum"] =  infoMap["channum"];
}

bool RemoteEncoder::SetChannelInfo(const InfoMap &infoMap)
{
    QStringList strlist( "SET_CHANNEL_INFO" );
    strlist << make_safe(infoMap["chanid"]);
    strlist << make_safe(infoMap["sourceid"]);
    strlist << make_safe(infoMap["oldchannum"]);
    strlist << make_safe(infoMap["callsign"]);
    strlist << make_safe(infoMap["channum"]);
    strlist << make_safe(infoMap["channame"]);
    strlist << make_safe(infoMap["XMLTV"]);

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toInt() != 0;

    return false;
}
