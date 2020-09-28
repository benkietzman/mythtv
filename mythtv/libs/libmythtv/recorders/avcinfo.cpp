// -*- Mode: c++ -*-

// MythTV headers
#include "avcinfo.h"
#ifndef GUID_ONLY
#include "firewiredevice.h"
#endif

AVCInfo::AVCInfo()
{
    m_unit_table.fill(0xff);
}

AVCInfo &AVCInfo::operator=(const AVCInfo &o)
{
    if (this == &o)
        return *this;

    m_port     = o.m_port;
    m_node     = o.m_node;
    m_guid     = o.m_guid;
    m_specid   = o.m_specid;
    m_vendorid = o.m_vendorid;
    m_modelid  = o.m_modelid;
    m_firmware_revision = o.m_firmware_revision;
    m_product_name = o.m_product_name;
    m_unit_table = o.m_unit_table;

    return *this;
}

bool AVCInfo::GetSubunitInfo(void)
{
    m_unit_table.fill(0xff);

    for (uint i = 0; i < 8; i++)
    {
        std::vector<uint8_t> cmd;
        std::vector<uint8_t> ret;

        cmd.push_back(FirewireDevice::kAVCStatusInquiryCommand);
        cmd.push_back(FirewireDevice::kAVCSubunitTypeUnit |
                      FirewireDevice::kAVCSubunitIdIgnore);
        cmd.push_back(FirewireDevice::kAVCUnitSubunitInfoOpcode);
        cmd.push_back((i<<4) | 0x07);
        cmd.push_back(0xFF);
        cmd.push_back(0xFF);
        cmd.push_back(0xFF);
        cmd.push_back(0xFF);

        if (!SendAVCCommand(cmd, ret, -1))
            return false;

        if (ret.size() >= 8)
        {
            m_unit_table[(i<<2)+0] = ret[4];
            m_unit_table[(i<<2)+1] = ret[5];
            m_unit_table[(i<<2)+2] = ret[6];
            m_unit_table[(i<<2)+3] = ret[7];
        }
    }

    return true;
}

bool AVCInfo::IsSubunitType(int subunit_type) const
{
    for (int subunit : m_unit_table)
    {
        if ((subunit != 0xff) &&
            (subunit & FirewireDevice::kAVCSubunitTypeUnit) == subunit_type)
        {
            return true;
        }
    }

    return false;
}

QString AVCInfo::GetSubunitInfoString(void) const
{
    QString str = "Subunit Types: ";

    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeVideoMonitor))
        str += "Video Monitor, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeAudio))
        str += "Audio, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypePrinter))
        str += "Printer, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeDiscRecorder))
        str += "Disk Recorder, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeTapeRecorder))
        str += "Tape Recorder, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeTuner))
        str += "Tuner, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeCA))
        str += "CA, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeVideoCamera))
        str += "Camera, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypePanel))
        str += "Panel, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeBulletinBoard))
        str += "Bulletin Board, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeCameraStorage))
        str += "Camera Storage, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeMusic))
        str += "Music, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeVendorUnique))
        str += "Vendor Unique, ";

    return str;
}
