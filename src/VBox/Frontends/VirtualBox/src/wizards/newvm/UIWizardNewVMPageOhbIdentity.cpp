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
    , m_pChkStealth(0)
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

    /* Stealth toggle: */
    m_pChkStealth = new QCheckBox(this);
    pMainLayout->addWidget(m_pChkStealth);

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
    if (m_pChkStealth)
        connect(m_pChkStealth, &QCheckBox::toggled,
                this, &UIWizardNewVMPageOhbIdentity::sltStealthToggled);

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
    if (m_pChkStealth)
    {
        m_pChkStealth->setText(UIWizardNewVM::tr("&Enable VMM stealth on first boot"));
        m_pChkStealth->setToolTip(UIWizardNewVM::tr(
            "Toggles the descriptor-table-exiting / TSC-offset compensation patches "
            "(HM/OhbHideDescTables, etc.). Requires an OpenHuizeBox-patched VBox build."));
    }
    if (m_pLblNote)
        m_pLblNote->setText(UIWizardNewVM::tr(
            "<small>Profile data lives in "
            "<code>modules/01_hardware_fingerprint/profiles/*.json</code>. "
            "Serials and UUIDs are derived deterministically from the VM name.</small>"));
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
    if (m_pChkStealth && !m_fUserModifiedStealth)
    {
        m_pChkStealth->blockSignals(true);
        m_pChkStealth->setChecked(false);
        m_pChkStealth->blockSignals(false);
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

void UIWizardNewVMPageOhbIdentity::sltStealthToggled(bool fChecked)
{
    m_fUserModifiedStealth = true;
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturnVoid(pWizard);
    pWizard->setOhbStealthMode(fChecked);
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
