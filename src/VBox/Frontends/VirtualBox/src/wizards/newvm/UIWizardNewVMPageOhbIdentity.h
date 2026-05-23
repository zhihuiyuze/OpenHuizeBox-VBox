/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageOhbIdentity class declaration.
 *
 * OpenHuizeBox: optional "Hardware Identity" step inserted into the stock
 * New VM wizard. The step is a thin profile-picker that lets the user pre-
 * select a hardware-identity preset BEFORE the VM is created, so that
 * once the VM is registered the post-create Settings page (UIMachine-
 * SettingsOhbIdentity) can pre-fill itself from the chosen profile and
 * stamp the freshly created VM's extradata in one shot.
 *
 * The full 17-field editor (SMBIOS, ACPI, CPUID brand, disk identifiers,
 * MAC OUI, VMM stealth toggles) deliberately does NOT live here -- a
 * wizard step is the wrong place for a 200-pixel-tall scrolling form.
 * What this step does is:
 *
 *   1. Show the profile combobox sourced from
 *        modules/01_hardware_fingerprint/profiles/*.json
 *      (same loader UIMachineSettingsOhbIdentity::profilesDirPath uses).
 *   2. Show a "Enable VMM stealth on first boot" checkbox.
 *   3. On wizard finish, stash {profile name, stealth_mode} into the
 *      UIWizardNewVM parent via setOhbProfileName/setOhbStealthMode so
 *      UIWizardNewVM::createVM() (or the OhbIdentity settings page on
 *      first open) can apply them.
 *
 * validateAfter() always returns true: the step is fully skippable.
 *
 * Lives between UIWizardNewVMHardwarePage and UIWizardNewVMSummaryPage;
 * see UIWizardNewVM::populatePages().
 */

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageOhbIdentity_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageOhbIdentity_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class QCheckBox;
class QComboBox;
class QLabel;
class QIRichTextLabel;

/** New Virtual Machine wizard: Hardware Identity (OpenHuizeBox) page. */
class UIWizardNewVMPageOhbIdentity : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs Hardware Identity page.
      * @param  strHelpKeyword  Brings the Help context keyword. */
    UIWizardNewVMPageOhbIdentity(const QString strHelpKeyword = QString());

private slots:

    /** Handles translation event. */
    virtual void sltRetranslateUI() RT_OVERRIDE RT_FINAL;

    /** User picked a different profile in the combobox. */
    void sltProfileSelectionChanged(int iIndex);
    /** User changed the Stealth Level dropdown (None / L1 / L2). */
    void sltStealthLevelChanged(int iIndex);

private:

    /** Prepares the page UI. */
    void prepare();
    /** Wires signal/slot connections. */
    void createConnections();

    /** Handles the page initialization. */
    virtual void initializePage() RT_OVERRIDE RT_FINAL;
    /** Page can always be left ("Next" is always enabled). */
    virtual bool isComplete() const RT_OVERRIDE RT_FINAL { return true; }

    /** Scans modules/01_hardware_fingerprint/profiles for *.json files. */
    void rescanProfiles();

    /** Returns the absolute path to the profiles directory.
      * Mirrors UIMachineSettingsOhbIdentity::profilesDirPath() so the two
      * surfaces stay in lock-step. */
    static QString profilesDirPath();

    /** @name Widgets
      * @{ */
        QIRichTextLabel *m_pLabel;
        QLabel          *m_pLblProfile;
        QComboBox       *m_pComboProfile;
        QLabel          *m_pLblStealthLevel;
        QComboBox       *m_pCmbStealthLevel;
        QLabel          *m_pLblNote;
    /** @} */

    /** True after the user touched a control, so initializePage()
      * does not overwrite a deliberate user pick on re-visit. */
    bool m_fUserModifiedProfile;
    bool m_fUserModifiedStealth;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageOhbIdentity_h */
