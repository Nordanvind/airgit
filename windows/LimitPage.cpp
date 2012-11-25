#include "stdafx.h"
#include "Resource.h"

#include "../client/SettingsManager.h"

#include "LimitPage.h"
#include "WinUtil.h"

PropPage::TextItem LimitPage::texts[] = {
	//limiter
	{ IDC_STRONGDC_UP_SPEED, ResourceManager::MAX_UPLOAD_RATE },
	{ IDC_STRONGDC_UP_SPEED1, ResourceManager::MAX_UPLOAD_RATE },
	{ IDC_SETTINGS_KBPS1, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS2, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS3, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS4, ResourceManager::KBPS },
	{ IDC_STRONGDC_DW_SPEED, ResourceManager::MAX_DOWNLOAD_RATE },
	{ IDC_STRONGDC_DW_SPEED1, ResourceManager::MAX_DOWNLOAD_RATE },
	{ IDC_TIME_LIMITING, ResourceManager::SET_ALTERNATE_LIMITING },
	{ IDC_STRONGDC_TO, ResourceManager::SETCZDC_TO },
	{ IDC_STRONGDC_TRANSFER_LIMITING2, ResourceManager::ALTERNATE_LIMITING },

	//uploads
	{ IDC_CZDC_SMALL_SLOTS, ResourceManager::SMALL_UP_SLOTS },
	{ IDC_CZDC_SMALL_SIZE, ResourceManager::SETCZDC_SMALL_FILES },
	{ IDC_SETTINGS_PARTIAL_SLOTS, ResourceManager::SETSTRONGDC_PARTIAL_SLOTS },		
	{ IDC_STATICb, ResourceManager::EXTRA_HUB_SLOTS },

	//other
	{ IDC_TRANSFER_ENCRYPTION_LBL, ResourceManager::TRANSFER_ENCRYPTION },

	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
}; 

PropPage::Item LimitPage::items[] = {
	//limiter
	{ IDC_MX_UP_SP_LMT_NORMAL, SettingsManager::MAX_UPLOAD_SPEED_MAIN, PropPage::T_INT },
	{ IDC_MX_DW_SP_LMT_NORMAL, SettingsManager::MAX_DOWNLOAD_SPEED_MAIN, PropPage::T_INT },
	{ IDC_TIME_LIMITING, SettingsManager::TIME_DEPENDENT_THROTTLE, PropPage::T_BOOL },
	{ IDC_MX_UP_SP_LMT_TIME, SettingsManager::MAX_UPLOAD_SPEED_ALTERNATE, PropPage::T_INT },
	{ IDC_MX_DW_SP_LMT_TIME, SettingsManager::MAX_DOWNLOAD_SPEED_ALTERNATE, PropPage::T_INT },
	{ IDC_BW_START_TIME, SettingsManager::BANDWIDTH_LIMIT_START, PropPage::T_INT },
	{ IDC_BW_END_TIME, SettingsManager::BANDWIDTH_LIMIT_END, PropPage::T_INT },

	//uploads
	{ IDC_EXTRA_SLOTS, SettingsManager::EXTRA_SLOTS, PropPage::T_INT },
	{ IDC_SMALL_FILE_SIZE, SettingsManager::SET_MINISLOT_SIZE, PropPage::T_INT },
	{ IDC_EXTRA_SLOTS2, SettingsManager::HUB_SLOTS, PropPage::T_INT },
	{ IDC_PARTIAL_SLOTS, SettingsManager::EXTRA_PARTIAL_SLOTS, PropPage::T_INT  },
	{ 0, 0, PropPage::T_END }
};

LRESULT LimitPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items);

	ctrlTransferEncryption.Attach(GetDlgItem(IDC_TRANSFER_ENCRYPTION));
	ctrlTransferEncryption.AddString(CTSTRING(DISABLED));
	ctrlTransferEncryption.AddString(CTSTRING(ENABLED));
	ctrlTransferEncryption.AddString(CTSTRING(ENCRYPTION_FORCED));
	ctrlTransferEncryption.SetCurSel(SETTING(TLS_MODE));

	//advanced limits start
	setMinMax(IDC_EXTRASPIN, 0, 10);
	setMinMax(IDC_PARTIAL_SLOTS_SPIN, 0, 10);
	setMinMax(IDC_SMALL_FILE_SIZE_SPIN, 64, 30000);
	setMinMax(IDC_EXTRA_SLOTS_SPIN, 3, 100);

	//limiter start
	setMinMax(IDC_UPLOADSPEEDSPIN, 0, 99999);
	setMinMax(IDC_DOWNLOADSPEEDSPIN, 0, 99999);
	setMinMax(IDC_UPLOADSPEEDSPIN_TIME, 0, 99999);
	setMinMax(IDC_DOWNLOADSPEEDSPIN_TIME, 0, 99999);

	timeCtrlBegin.Attach(GetDlgItem(IDC_BW_START_TIME));
	timeCtrlEnd.Attach(GetDlgItem(IDC_BW_END_TIME));

	timeCtrlBegin.AddString(CTSTRING(MIDNIGHT));
	timeCtrlEnd.AddString(CTSTRING(MIDNIGHT));
	for (int i = 1; i < 12; ++i)
	{
		timeCtrlBegin.AddString((Util::toStringW(i) + CTSTRING(AM)).c_str());
		timeCtrlEnd.AddString((Util::toStringW(i) + CTSTRING(AM)).c_str());
	}
	timeCtrlBegin.AddString(CTSTRING(NOON));
	timeCtrlEnd.AddString(CTSTRING(NOON));
	for (int i = 1; i < 12; ++i)
	{
		timeCtrlBegin.AddString((Util::toStringW(i) + CTSTRING(PM)).c_str());
		timeCtrlEnd.AddString((Util::toStringW(i) + CTSTRING(PM)).c_str());
	}

	timeCtrlBegin.SetCurSel(SETTING(BANDWIDTH_LIMIT_START));
	timeCtrlEnd.SetCurSel(SETTING(BANDWIDTH_LIMIT_END));

	timeCtrlBegin.Detach();
	timeCtrlEnd.Detach();

	fixControls();

	// Do specialized reading here

	return TRUE;
}

void LimitPage::write()
{
	PropPage::write((HWND)*this, items);

	// Do specialized writing here
	// settings->set(XX, YY);

	timeCtrlBegin.Attach(GetDlgItem(IDC_BW_START_TIME));
	timeCtrlEnd.Attach(GetDlgItem(IDC_BW_END_TIME));
	settings->set(SettingsManager::BANDWIDTH_LIMIT_START, timeCtrlBegin.GetCurSel());
	settings->set(SettingsManager::BANDWIDTH_LIMIT_END, timeCtrlEnd.GetCurSel());
	timeCtrlBegin.Detach();
	timeCtrlEnd.Detach(); 

	SettingsManager::getInstance()->set(SettingsManager::TLS_MODE, ctrlTransferEncryption.GetCurSel());
}

void LimitPage::fixControls() {
	bool state = (IsDlgButtonChecked(IDC_TIME_LIMITING) != 0);
	::EnableWindow(GetDlgItem(IDC_BW_START_TIME), state);
	::EnableWindow(GetDlgItem(IDC_BW_END_TIME), state);
	::EnableWindow(GetDlgItem(IDC_MX_UP_SP_LMT_TIME), state);
	::EnableWindow(GetDlgItem(IDC_UPLOADSPEEDSPIN_TIME), state);
	::EnableWindow(GetDlgItem(IDC_MX_DW_SP_LMT_TIME), state);
	::EnableWindow(GetDlgItem(IDC_DOWNLOADSPEEDSPIN_TIME), state);
	::EnableWindow(GetDlgItem(IDC_STRONGDC_DW_SPEED1), state);
	::EnableWindow(GetDlgItem(IDC_STRONGDC_UP_SPEED1), state);
}

LRESULT LimitPage::onChangeCont(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	switch (wID) {
	case IDC_TIME_LIMITING:
		fixControls();
		break;
	}
	return true;
}