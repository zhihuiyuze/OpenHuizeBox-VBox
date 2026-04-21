/** @file
 * OpenHuizeBox - Shared helper for injecting the "Realistic Hardware Identity"
 * profile-picker groupbox into any UIMachineSettings* tab.
 *
 * Header-only (no .cpp) so we don't have to touch kmk Makefile.kmk.
 * Included once per settings page that wants the groupbox.
 */

#ifndef FEQT_INCLUDED_SRC_settings_machine_UIOhbHelpers_h
#define FEQT_INCLUDED_SRC_settings_machine_UIOhbHelpers_h

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QRandomGenerator>
#include <QVBoxLayout>
#include <QWidget>

#include "CMachine.h"

namespace UIOhb
{

/** Attach an "OpenHuizeBox - Realistic Hardware Identity" groupbox to the
 *  given grid layout at the given row. subtitle is cosmetic and tells the
 *  user which class of signals this tab primarily affects.
 *
 *  machineProvider is a callable that returns the live CMachine AT CLICK
 *  time (not construction time) - because UIMachineSettings* pages
 *  populate m_machine after prepareWidgets() runs.
 */
/** Builds the groupbox widget and returns it. Caller places it in their
 *  layout (QGridLayout / QVBoxLayout / QHBoxLayout - any). */
template<typename MachineProvider>
inline QGroupBox* buildProfileBox(QWidget *pParent,
                                  MachineProvider machineProvider,
                                  const QString &strSubtitle)
{
    if (!pParent)
        return nullptr;

    QGroupBox *pBox = new QGroupBox(QString::fromUtf8(
        "OpenHuizeBox - Realistic Hardware Identity"), pParent);
    QVBoxLayout *pLay = new QVBoxLayout(pBox);

    QLabel *pInfo = new QLabel(QString::fromUtf8(
        "<b>Applies to:</b> %1<br/>"
        "Pick a hardware-identity profile to apply realistic vendor strings, "
        "MAC OUI, disk model/serial and ACPI/SMBIOS tables to this VM. For "
        "authorised privacy-audit research only - see Governance.").arg(strSubtitle), pBox);
    pInfo->setWordWrap(true);
    pInfo->setStyleSheet(QString::fromUtf8("color:#555;"));
    pLay->addWidget(pInfo);

    /*
     * Master Stealth switch - binds the per-VM CFGM key
     * VBoxInternal/HM/OhbStealth that the fork's HMR3Init reads at
     * VM start. Checked -> fork enables VMX descriptor-table exiting,
     * Windows-plausible fake IDTR/GDTR, and the per-device PCI VID
     * override reader path. Unchecked -> stock VBox speed, zero
     * VM-exit overhead, VM is detectable.
     */
    QCheckBox *pStealthCb = new QCheckBox(QString::fromUtf8(
        "Stealth mode  (anti-detection)"), pBox);
    pStealthCb->setToolTip(QString::fromUtf8(
        "When checked:\n"
        "  - VT-x descriptor-table exiting is enabled; SIDT / SGDT return\n"
        "    Windows-kernel-plausible IDT / GDT base addresses (Red Pill defeated).\n"
        "  - Per-device PCI Vendor / Device ID overrides take effect on VM start.\n"
        "  - All profile-driven SMBIOS / ACPI / disk / MAC shaping applies.\n"
        "  - Minor VM-exit overhead on descriptor-table instructions.\n"
        "\n"
        "When unchecked:\n"
        "  - Stock Oracle VirtualBox - full native speed, no extra VM-exits.\n"
        "  - This VM is detectable by Pafish / Al-Khaser / VMAware.\n"
        "\n"
        "Toggling writes VBoxInternal/HM/OhbStealth; applies on next power-on."));
    QLabel *pStealthDesc = new QLabel(QString::fromUtf8(
        "<span style='color:#666;'>Master switch for the OpenHuizeBox fork patches. "
        "Off &#x2192; stock speed, detectable. "
        "On &#x2192; full stealth stack, slight VM-exit overhead. "
        "Change takes effect on next power-on.</span>"), pBox);
    pStealthDesc->setWordWrap(true);
    pStealthDesc->setContentsMargins(20, 0, 0, 6);

    /* Seed checkbox from current extradata. */
    {
        CMachine m = machineProvider();
        if (!m.isNull())
        {
            QString strVal = m.GetExtraData(QString::fromUtf8("VBoxInternal/HM/OhbStealth"));
            pStealthCb->setChecked(strVal == QString::fromUtf8("1"));
        }
    }

    QObject::connect(pStealthCb, &QCheckBox::toggled, pBox,
        [machineProvider](bool fChecked)
        {
            CMachine m = machineProvider();
            if (!m.isNull())
                m.SetExtraData(QString::fromUtf8("VBoxInternal/HM/OhbStealth"),
                               QString::fromUtf8(fChecked ? "1" : "0"));
        });

    pLay->addWidget(pStealthCb);
    pLay->addWidget(pStealthDesc);

    /* Visual separator between the stealth master and the profile picker. */
    QFrame *pSep = new QFrame(pBox);
    pSep->setFrameShape(QFrame::HLine);
    pSep->setFrameShadow(QFrame::Sunken);
    pLay->addWidget(pSep);

    QComboBox *pCombo = new QComboBox(pBox);
    const QString strAppDir = QCoreApplication::applicationDirPath();
    QDir profilesDir(strAppDir + QString::fromUtf8("/../modules/01_hardware_fingerprint/profiles"));
    for (const QString &p : profilesDir.entryList(QStringList() << QString::fromUtf8("*.json"), QDir::Files))
        pCombo->addItem(p);
    if (pCombo->count() == 0)
        pCombo->addItem(QString::fromUtf8("(no profiles found)"));
    pLay->addWidget(pCombo);

    QPushButton *pApply = new QPushButton(QString::fromUtf8("Apply profile to this VM now"), pBox);
    pLay->addWidget(pApply);

    QLabel *pStatus = new QLabel(pBox);
    pStatus->setWordWrap(true);
    pLay->addWidget(pStatus);

    QObject::connect(pApply, &QPushButton::clicked, pBox, [pCombo, pStatus, machineProvider, strAppDir, profilesDir]() mutable {
        CMachine machine = machineProvider();
        QString strVmName = machine.isNull() ? QString() : machine.GetName();
        if (strVmName.isEmpty()) {
            pStatus->setText(QString::fromUtf8(
                "<span style='color:#b00;'>Cannot resolve VM name (m_machine null - "
                "open Settings from an existing VM, not the global preferences).</span>"));
            return;
        }
        QString strChosen = pCombo->currentText();
        if (strChosen.startsWith(QChar('('))) return;
        QFile f(profilesDir.filePath(strChosen));
        if (!f.open(QIODevice::ReadOnly)) {
            pStatus->setText(QString::fromUtf8("<span style='color:#b00;'>Cannot read profile.</span>"));
            return;
        }
        QJsonParseError jpe;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &jpe);
        f.close();
        if (jpe.error != QJsonParseError::NoError) {
            pStatus->setText(QString::fromUtf8("<span style='color:#b00;'>Profile JSON parse error.</span>"));
            return;
        }
        QJsonObject pd    = doc.object();
        QJsonObject extra = pd.value(QString::fromUtf8("extradata")).toObject();
        QJsonArray  pmod  = pd.value(QString::fromUtf8("modifyvm_args")).toArray();
        QJsonArray  pool  = pd.value(QString::fromUtf8("mac_oui_pool")).toArray();
        QJsonObject dids  = pd.value(QString::fromUtf8("disk_identifiers")).toObject();
        QJsonObject dtmpl = pd.value(QString::fromUtf8("disk_extradata_template")).toObject();
        const QString strVbm = strAppDir + QString::fromUtf8("/VBoxManage.exe");
        auto run = [&](const QStringList &args) -> bool {
            QProcess pr; pr.start(strVbm, args); pr.waitForFinished(15000);
            return pr.exitCode() == 0;
        };
        run(QStringList() << QString::fromUtf8("modifyvm") << strVmName
                          << QString::fromUtf8("--audio-driver")     << QString::fromUtf8("none")
                          << QString::fromUtf8("--paravirtprovider") << QString::fromUtf8("none")
                          << QString::fromUtf8("--nested-hw-virt")   << QString::fromUtf8("off"));
        for (const QJsonValue &v : pmod) {
            QString a = v.toString();
            QStringList mv; mv << QString::fromUtf8("modifyvm") << strVmName;
            int eq = a.indexOf(QChar('='));
            if (eq >= 0) mv << a.left(eq) << a.mid(eq + 1); else mv << a;
            run(mv);
        }
        QString strMac;
        if (!pool.isEmpty()) {
            QString oui = pool[QRandomGenerator::global()->bounded(pool.size())].toString();
            oui.remove(QChar(':'));
            QString nic;
            for (int i = 0; i < 6; ++i)
                nic += QString::fromLatin1("%1").arg(QRandomGenerator::global()->bounded(16), 1, 16);
            strMac = (oui + nic).toUpper();
            run(QStringList() << QString::fromUtf8("modifyvm") << strVmName
                              << QString::fromUtf8("--macaddress1") << strMac);
        }
        int cApplied = 0;
        for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
            QString v = it.value().toString();
            if (v == QString::fromUtf8("__RUNTIME_PER_VM_UUID__")) {
                v.clear();
                static const char hex[] = "0123456789abcdef";
                for (int i = 0; i < 32; ++i) {
                    v += QChar(hex[QRandomGenerator::global()->bounded(16)]);
                    if (i == 7 || i == 11 || i == 15 || i == 19) v += QChar('-');
                }
            }
            if (run(QStringList() << QString::fromUtf8("setextradata") << strVmName << it.key() << v))
                ++cApplied;
        }
        const QString strHddModel = dids.value(QString::fromUtf8("hdd_model")).toString();
        QString strHddSerial = dids.value(QString::fromUtf8("hdd_serial")).toString();
        if (strHddSerial == QString::fromUtf8("__RUNTIME_PER_VM_SERIAL__")) {
            strHddSerial.clear();
            for (int i = 0; i < 16; ++i)
                strHddSerial += QString::fromLatin1("%1").arg(QRandomGenerator::global()->bounded(16), 1, 16).toUpper();
        }
        int cDiskApplied = 0;
        for (auto it = dtmpl.constBegin(); it != dtmpl.constEnd(); ++it) {
            QString val = it.value().toString();
            val.replace(QString::fromUtf8("{hdd_model}"),  strHddModel);
            val.replace(QString::fromUtf8("{hdd_serial}"), strHddSerial);
            if (run(QStringList() << QString::fromUtf8("setextradata") << strVmName << it.key() << val))
                ++cDiskApplied;
        }
        pStatus->setText(QString::fromUtf8(
            "<span style='color:#0a7a0a;'>Applied <b>%1</b> to VM <b>%2</b>: "
            "%3 extradata + %4 disk, MAC <code>%5</code>. Close and reopen Settings to see "
            "the refreshed values in the Advanced / Storage tabs.</span>")
            .arg(strChosen).arg(strVmName).arg(cApplied).arg(cDiskApplied)
            .arg(strMac.isEmpty() ? QString::fromUtf8("(unchanged)") : strMac));
    });

    return pBox;
}

/** Convenience overload for QGridLayout call sites — places the box at
 *  (row, 0) spanning 2 columns. */
template<typename MachineProvider>
inline void attachProfileBox(QGridLayout *pLayout, int iRow, QWidget *pParent,
                             MachineProvider machineProvider,
                             const QString &strSubtitle)
{
    QGroupBox *pBox = buildProfileBox(pParent, machineProvider, strSubtitle);
    if (pLayout && pBox)
        pLayout->addWidget(pBox, iRow, 0, 1, 2);
}

} /* namespace UIOhb */

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIOhbHelpers_h */
