/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageOhbIdentity class implementation.
 *
 * See header for design rationale.
 */

/* Qt includes: */
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QLabel>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UITranslationEventListener.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageOhbIdentity.h"


UIWizardNewVMPageOhbIdentity::UIWizardNewVMPageOhbIdentity(const QString strHelpKeyword /* = QString() */)
    : UINativeWizardPage(strHelpKeyword)
    , m_pLabel(0)
    , m_pLblProfile(0)
    , m_pComboProfile(0)
    , m_pLblStealthLevel(0)
    , m_pCmbStealthLevel(0)
    , m_pLblNote(0)
    , m_fUserModifiedProfile(false)
    , m_fUserModifiedStealth(false)
{
    prepare();
}

void UIWizardNewVMPageOhbIdentity::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    /* Top description label: */
    m_pLabel = new QIRichTextLabel(this);
    pMainLayout->addWidget(m_pLabel);

    /* Profile row: */
    QHBoxLayout *pRow = new QHBoxLayout;
    m_pLblProfile   = new QLabel(this);
    m_pComboProfile = new QComboBox(this);
    m_pComboProfile->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    pRow->addWidget(m_pLblProfile);
    pRow->addWidget(m_pComboProfile, 1);
    pMainLayout->addLayout(pRow);

    /* Stealth Level dropdown (None / L1 / L2): */
    {
        QHBoxLayout *pLvlRow = new QHBoxLayout;
        m_pLblStealthLevel = new QLabel(this);
        m_pCmbStealthLevel = new QComboBox(this);
        m_pCmbStealthLevel->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        /* Item data is the persisted stealth_mode hint:
         *   "none" -> no L1/L2 stealth   (stealth_mode = false)
         *   "l1"   -> SMBIOS/ACPI/MAC/Disk identity only (stealth_mode = false)
         *   "l2"   -> L1 + VMM descriptor-table / TSC / PCI override (stealth_mode = true)
         * Settings tab Stealth Level preset uses the same convention. */
        m_pCmbStealthLevel->addItem(QString(), QString("none"));
        m_pCmbStealthLevel->addItem(QString(), QString("l1"));
        m_pCmbStealthLevel->addItem(QString(), QString("l2"));
        pLvlRow->addWidget(m_pLblStealthLevel);
        pLvlRow->addWidget(m_pCmbStealthLevel, 1);
        pMainLayout->addLayout(pLvlRow);
    }

    /* Foot-note: */
    m_pLblNote = new QLabel(this);
    m_pLblNote->setWordWrap(true);
    pMainLayout->addWidget(m_pLblNote);

    pMainLayout->addStretch();

    rescanProfiles();
    createConnections();
}

void UIWizardNewVMPageOhbIdentity::createConnections()
{
    if (m_pComboProfile)
        connect(m_pComboProfile, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &UIWizardNewVMPageOhbIdentity::sltProfileSelectionChanged);
    if (m_pCmbStealthLevel)
        connect(m_pCmbStealthLevel, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &UIWizardNewVMPageOhbIdentity::sltStealthLevelChanged);

    connect(&translationEventListener(), &UITranslationEventListener::sigRetranslateUI,
            this, &UIWizardNewVMPageOhbIdentity::sltRetranslateUI);
}

void UIWizardNewVMPageOhbIdentity::sltRetranslateUI()
{
    setTitle(UIWizardNewVM::tr("Hardware Identity (OpenHuizeBox)"));

    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr(
            "Optional: pre-select a realistic hardware identity profile (SMBIOS, ACPI, "
            "disk model, MAC OUI). The chosen profile will be applied to the new VM "
            "immediately after creation. You can change or clear it later from "
            "Settings > Hardware Identity. Leave at <i>(none)</i> to skip."));

    if (m_pLblProfile)
        m_pLblProfile->setText(UIWizardNewVM::tr("&Profile:"));
    if (m_pLblStealthLevel)
        m_pLblStealthLevel->setText(UIWizardNewVM::tr("&Stealth Level:"));
    if (m_pCmbStealthLevel)
    {
        m_pCmbStealthLevel->setItemText(0,
            UIWizardNewVM::tr("None  -  raw VBox (no performance cost, VM detectable)"));
        m_pCmbStealthLevel->setItemText(1,
            UIWizardNewVM::tr("L1 Light  -  SMBIOS / ACPI / MAC / Disk identity (no performance cost)"));
        m_pCmbStealthLevel->setItemText(2,
            UIWizardNewVM::tr("L2 Deep  -  L1 + descriptor-table spoof + TSC compensation + PCI override (~3-5%% perf cost)"));
        m_pCmbStealthLevel->setToolTip(UIWizardNewVM::tr(
            "L1: changes only the identity strings the guest OS reads (BIOS, "
            "board, chassis, MAC OUI, disk model). No VMM-level changes. Safe "
            "default for most use cases.\n"
            "L2: in addition to L1, also enables VMM-level features that defeat "
            "Pafish / Al-Khaser style instruction-based detection (SIDT / SGDT "
            "Red Pill, RDTSC timing, VEN_80EE PCI scan). Costs roughly 3-5%% on "
            "CPU-bound workloads."));
    }
    if (m_pLblNote)
        m_pLblNote->setText(UIWizardNewVM::tr(
            "<small>Profile data lives in "
            "<code>modules/01_hardware_fingerprint/profiles/*.json</code>. "
            "Serials and UUIDs are derived deterministically from the VM name. "
            "You can edit any individual field or change the Stealth Level "
            "later from Settings &rarr; Hardware Identity.</small>"));
}

void UIWizardNewVMPageOhbIdentity::initializePage()
{
    sltRetranslateUI();

    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturnVoid(pWizard);

    /* Only seed defaults on first visit -- once the user has touched anything,
     * leave their choices alone (mirrors UIWizardNewVMHardwarePage's
     * m_userModifiedParameters pattern). */
    if (m_pComboProfile && !m_fUserModifiedProfile)
    {
        m_pComboProfile->blockSignals(true);
        /* Default to "(none)" so the step is genuinely opt-in. */
        const int iIdx = m_pComboProfile->findData(QString(), Qt::UserRole);
        m_pComboProfile->setCurrentIndex(iIdx >= 0 ? iIdx : 0);
        m_pComboProfile->blockSignals(false);
        pWizard->setOhbProfileName(QString());
    }
    if (m_pCmbStealthLevel && !m_fUserModifiedStealth)
    {
        /* Default selection: L1 Light. Safe baseline that defeats every
         * string-matching detection vector with zero runtime overhead. */
        m_pCmbStealthLevel->blockSignals(true);
        const int iL1 = m_pCmbStealthLevel->findData(QString("l1"));
        m_pCmbStealthLevel->setCurrentIndex(iL1 >= 0 ? iL1 : 0);
        m_pCmbStealthLevel->blockSignals(false);
        /* L1 sets stealth_mode = false (VMM-level features stay off). */
        pWizard->setOhbStealthMode(false);
    }
}

void UIWizardNewVMPageOhbIdentity::sltProfileSelectionChanged(int /* iIndex */)
{
    m_fUserModifiedProfile = true;
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturnVoid(pWizard);
    AssertReturnVoid(m_pComboProfile);
    /* The combobox stores the filename in UserRole; itemText() is the display name. */
    const QString strProfile = m_pComboProfile->currentData(Qt::UserRole).toString();
    pWizard->setOhbProfileName(strProfile);
}

void UIWizardNewVMPageOhbIdentity::sltStealthLevelChanged(int /* iIndex */)
{
    m_fUserModifiedStealth = true;
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturnVoid(pWizard);
    AssertReturnVoid(m_pCmbStealthLevel);
    /* Map: only L2 sets the VMM-stealth flag (HM/OhbStealth=1 on first boot).
     * L1 keeps stealth_mode=false but the post-create OhbIdentity settings
     * page will still apply the chosen hardware identity profile. */
    const QString strLvl = m_pCmbStealthLevel->currentData().toString();
    pWizard->setOhbStealthMode(strLvl == QLatin1String("l2"));
}

void UIWizardNewVMPageOhbIdentity::rescanProfiles()
{
    if (!m_pComboProfile)
        return;
    m_pComboProfile->clear();
    /* Index 0 is the explicit "(none)" choice -- stored as empty string. */
    m_pComboProfile->addItem(UIWizardNewVM::tr("(none)"), QString());

    QDir d(profilesDirPath());
    const QStringList files = d.entryList(QStringList() << "*.json", QDir::Files);
    for (const QString &f : files)
    {
        /* Display the bare filename; the wizard stores the same. */
        m_pComboProfile->addItem(f, f);
    }
}

/* static */
QString UIWizardNewVMPageOhbIdentity::profilesDirPath()
{
    /* Kept in sync with UIMachineSettingsOhbIdentity::profilesDirPath(). */
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + "/../modules/01_hardware_fingerprint/profiles",
        appDir + "/modules/01_hardware_fingerprint/profiles",
        appDir + "/../../modules/01_hardware_fingerprint/profiles"
    };
    for (const QString &p : candidates)
        if (QDir(p).exists())
            return QDir::cleanPath(p);
    return candidates.first();
}
