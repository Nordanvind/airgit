/* 
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdafx.h"

#include "../client/Util.h"
#include "../client/version.h"
#include "../client/ClientManager.h"
#include "../client/FavoriteManager.h"

#include "Resource.h"
#include "ShareDirectories.h"
#include "WinUtil.h"
#include "LineDlg.h"
#include "PropertiesDlg.h"
#include "TempShare_dlg.h"
#include "SharePageDlg.h"
#include "MainFrm.h"

#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/algorithm/remove_if.hpp>
#include <boost/range/algorithm/find_if.hpp>

#define curProfile parent->getCurProfile()

PropPage::TextItem ShareDirectories::texts[] = {
	{ IDC_SETTINGS_SHARED_DIRECTORIES, ResourceManager::SETTINGS_SHARED_DIRECTORIES },
	{ IDC_REMOVE_DIR, ResourceManager::REMOVE },
	{ IDC_ADD_DIR, ResourceManager::SETTINGS_ADD_FOLDER },
	{ IDC_RENAME_DIR, ResourceManager::SETTINGS_RENAME_FOLDER },
	{ IDC_SHARE_PROFILE_NOTE, ResourceManager::SETTINGS_SHARE_PROFILE_NOTE },
	{ IDC_SHOW_TREE, ResourceManager::SHOW_DIRECTORY_TREE },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

ShareDirectories::ShareDirectories(SharePageBase* aParent, SettingsManager *s) : parent(aParent), SettingTab(s), ft(nullptr) { }

LRESULT ShareDirectories::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SettingTab::translate((HWND)(*this), texts);

	ctrlDirectories.Attach(GetDlgItem(IDC_DIRECTORIES));
	ctrlDirectories.SetExtendedListViewStyle(LVS_EX_LABELTIP | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_CHECKBOXES);


	CheckDlgButton(IDC_SHOW_TREE, SETTING(USE_OLD_SHARING_UI) ? BST_UNCHECKED : BST_CHECKED);

	GetDlgItem(IDC_TREE1).ShowWindow((SETTING(USE_OLD_SHARING_UI)) ? SW_HIDE : SW_SHOW);
	GetDlgItem(IDC_DIRECTORIES).ShowWindow((SETTING(USE_OLD_SHARING_UI)) ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_ADD_DIR).ShowWindow((SETTING(USE_OLD_SHARING_UI)) ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_REMOVE_DIR).ShowWindow((SETTING(USE_OLD_SHARING_UI)) ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_RENAME_DIR).ShowWindow((SETTING(USE_OLD_SHARING_UI)) ? SW_SHOW : SW_HIDE);

	if(SETTING(USE_OLD_SHARING_UI)) {
		// Prepare shared dir list
		ctrlDirectories.InsertColumn(0, CTSTRING(VIRTUAL_NAME), LVCFMT_LEFT, 80, 0);
		ctrlDirectories.InsertColumn(1, CTSTRING(DIRECTORY), LVCFMT_LEFT, 197, 1);
		ctrlDirectories.InsertColumn(2, CTSTRING(SIZE), LVCFMT_RIGHT, 90, 2);
	}

	ShareManager::getInstance()->getShares(shareDirs);
	showProfile();

	return TRUE;
}

LRESULT ShareDirectories::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
	if(reinterpret_cast<HWND>(wParam) == ctrlDirectories) { 
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if(pt.x == -1 && pt.y == -1) {
			WinUtil::getContextMenuPos(ctrlDirectories, pt);
		}

		int selectedDirs = ctrlDirectories.GetSelectedCount();
		if (selectedDirs > 0) {
			int i = -1;
			bool hasRemoved=false, hasAdded=false;
			while((i = ctrlDirectories.GetNextItem(i, LVNI_SELECTED)) != -1) {
				auto sdi = (ShareDirInfo*)ctrlDirectories.GetItemData(i);
				if (sdi->diffState == ShareDirInfo::DIFF_REMOVED) {
					hasRemoved = true;
				} else {
					hasAdded = true;
				}
			}

			OMenu menu;
			menu.CreatePopupMenu();
			if (hasAdded)
				menu.AppendMenu(MF_STRING, IDC_REMOVE_DIR, CTSTRING(REMOVE));
			if (hasRemoved)
				menu.AppendMenu(MF_STRING, IDC_ADD_DIR, CTSTRING(ADD_THIS_PROFILE));
			if (selectedDirs == 1) {
				auto path = Text::toT(((ShareDirInfo*)ctrlDirectories.GetItemData(ctrlDirectories.GetNextItem(i, LVNI_SELECTED)))->path);
				menu.appendItem(TSTRING(OPEN_FOLDER), [path] { WinUtil::openFolder(path); });
			}
			menu.open(m_hWnd, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt);
			return TRUE;
		}
	}
	bHandled = FALSE;
	return FALSE; 
}

/*void ShareDirectories::onProfileChanged(ProfileToken aProfile) {
	curProfile = aProfile;
	showProfile();
	fixControls();
}*/

ShareDirInfo::List ShareDirectories::getViewItems(ProfileToken aProfile, bool getDiffItems /*false*/) {
	ShareDirInfo::List ret;

	//add shared dirs that haven't been removed now
	auto& dirs = shareDirs[aProfile];
	copy_if(dirs.begin(), dirs.end(), back_inserter(ret), [this](const ShareDirInfoPtr& sdi) { 
		return find(removeDirs.begin(), removeDirs.end(), sdi) == removeDirs.end(); 
	});

	//add all added dirs for this profile
	copy_if(newDirs.begin(), newDirs.end(), back_inserter(ret), [aProfile](const ShareDirInfoPtr& sdi) { 
		return compare(sdi->profile, aProfile) == 0; 
	});


	if (getDiffItems && aProfile != SP_DEFAULT) {
		//add items from the default profile that are missing from this profile
		auto defaultItems = getViewItems(SP_DEFAULT);
		for(const auto& j: defaultItems) {
			if (boost::find_if(ret, [j](const ShareDirInfoPtr& aDir) { return aDir->path == j->path; }) == ret.end()) {
				auto sdi = new ShareDirInfo(j->vname, aProfile, j->path, j->incoming);
				sdi->size = j->size;
				sdi->diffState = ShareDirInfo::DIFF_REMOVED;
				tempViewItems.push_back(sdi);
				ret.push_back(sdi);
			}
		}

		for(const auto& j: ret) {
			if (j->diffState != ShareDirInfo::DIFF_REMOVED && boost::find_if(defaultItems, [j](const ShareDirInfoPtr& aDir) { return aDir->path == j->path; }) == defaultItems.end()) {
				j->diffState = ShareDirInfo::DIFF_ADDED;
			}
		}
	}

	//sort(ret.begin(), ret.end(), ShareDirInfo::Sort());
	return ret;
}

StringSet ShareDirectories::getExcludedDirs() {
	StringList tmp;
	ShareManager::getInstance()->getExcludes(curProfile, tmp);

	//don't list removed items
	StringSet ret;
	for (const auto& j: tmp) {
		if (boost::find_if(excludedRemove, [this, j](pair<ProfileToken, string>& pp) { return pp.first == curProfile && pp.second == j; }) == excludedRemove.end()) {
			ret.insert(j);
		}
	}

	//add new items
	for (const auto& j: excludedAdd) {
		if (j.first == curProfile)
			ret.insert(j.second);
	}
	return ret;
}

static int sort(LPARAM lParam1, LPARAM lParam2, int column) {
	auto left = (ShareDirInfo*)lParam1;
	auto right = (ShareDirInfo*)lParam2;

	//removed always go last
	if (left->diffState == ShareDirInfo::DIFF_REMOVED && right->diffState != ShareDirInfo::DIFF_REMOVED) return 1;
	if (left->diffState != ShareDirInfo::DIFF_REMOVED && right->diffState == ShareDirInfo::DIFF_REMOVED) return -1;

	//added are first
	if (left->diffState != ShareDirInfo::DIFF_ADDED && right->diffState == ShareDirInfo::DIFF_ADDED) return 1;
	if (left->diffState == ShareDirInfo::DIFF_ADDED && right->diffState != ShareDirInfo::DIFF_ADDED) return -1;
	
	//don't return values over 1 so that ExListViewEx won't do its "tricks"
	if (column == 0) {
		return min(1, Util::DefaultSort(Text::toT(left->vname).c_str(), Text::toT(right->vname).c_str()));
	} else if (column == 1) {
		return min(1, Util::DefaultSort(Text::toT(left->path).c_str(), Text::toT(right->path).c_str()));
	} else {
		return compare(right->size, left->size);
	}
}

void ShareDirectories::showProfile() {
	deleteTempViewItems();
	if (ft.get()) {
		ft->Clear();
		ft->UnsubclassWindow(true);
		//ft->DestroyWindow();
		ft.reset();
	} else {
		ctrlDirectories.DeleteAllItems();
	}

	GetDlgItem(IDC_TREE1).ShowWindow((SETTING(USE_OLD_SHARING_UI)) ? SW_HIDE : SW_SHOW);
	GetDlgItem(IDC_DIRECTORIES).ShowWindow((SETTING(USE_OLD_SHARING_UI)) ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_ADD_DIR).ShowWindow((SETTING(USE_OLD_SHARING_UI)) ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_REMOVE_DIR).ShowWindow((SETTING(USE_OLD_SHARING_UI)) ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_RENAME_DIR).ShowWindow((SETTING(USE_OLD_SHARING_UI)) ? SW_SHOW : SW_HIDE);

	if(SETTING(USE_OLD_SHARING_UI)) {
		// Prepare shared dir list
		auto shares = getViewItems(curProfile, true);
		for(auto& sdi: shares) {
			int i = ctrlDirectories.insert(ctrlDirectories.GetItemCount(), Text::toT(sdi->vname), 0, (LPARAM)sdi.get());
			ctrlDirectories.SetItemText(i, 1, Text::toT(sdi->path).c_str());
			ctrlDirectories.SetItemText(i, 2, Util::formatBytesW(sdi->size).c_str());
			ctrlDirectories.SetCheckState(i, sdi->incoming);
		}
		ctrlDirectories.setSort(0, ExListViewCtrl::SORT_FUNC, true, sort);
	} else {
		ft.reset(new FolderTree(this));
		ft->SubclassWindow(GetDlgItem(IDC_TREE1));
		ft->PopulateTree();
	}
}

LRESULT ShareDirectories::onCustomDrawList(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/) {
	LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)pnmh;
	switch(cd->nmcd.dwDrawStage) {

		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT: {
			auto ii = reinterpret_cast<ShareDirInfo*>(cd->nmcd.lItemlParam);
			if (ii->diffState == ShareDirInfo::DIFF_REMOVED) {
				cd->clrText = RGB(225, 225, 225);
			} else if (ii->diffState == ShareDirInfo::DIFF_ADDED) {
				cd->clrText = RGB(0, 180, 13);
			}
			return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
		}

		default:
			return CDRF_DODEFAULT;
	}
}

LRESULT ShareDirectories::onClickedShowTree(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto useOldInterface = IsDlgButtonChecked(IDC_SHOW_TREE) != BST_CHECKED;
	SettingsManager::getInstance()->set(SettingsManager::USE_OLD_SHARING_UI, useOldInterface);
	if(useOldInterface) {
		// Prepare shared dir list
		ctrlDirectories.InsertColumn(0, CTSTRING(VIRTUAL_NAME), LVCFMT_LEFT, 80, 0);
		ctrlDirectories.InsertColumn(1, CTSTRING(DIRECTORY), LVCFMT_LEFT, 197, 1);
		ctrlDirectories.InsertColumn(2, CTSTRING(SIZE), LVCFMT_RIGHT, 90, 2);
	} else {
		ctrlDirectories.DeleteColumn(0);
		ctrlDirectories.DeleteColumn(0);
		ctrlDirectories.DeleteColumn(0);
	}

	showProfile();
	return 0;
}

LRESULT ShareDirectories::onColumnClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/) {
	
	NMLISTVIEW* l = (NMLISTVIEW*)pnmh;
	
	if(l->iSubItem == ctrlDirectories.getSortColumn()) {
		if(!ctrlDirectories.isAscending()) { //change the direction of sorting by clicking on a already sorted column
			ctrlDirectories.setSort(-1, ctrlDirectories.getSortType());
		} else {
			ctrlDirectories.setSortDirection(false);
		}
	} else {
		ctrlDirectories.setSort(l->iSubItem, ExListViewCtrl::SORT_FUNC, true, sort);
	}
	return 0;
}

LRESULT ShareDirectories::onDropFiles(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/){
	HDROP drop = (HDROP)wParam;
	TCHAR buf[MAX_PATH];
	UINT nrFiles;
	
	nrFiles = DragQueryFile(drop, (UINT)-1, NULL, 0);
	
	for(UINT i = 0; i < nrFiles; ++i){
		if(DragQueryFile(drop, i, buf, MAX_PATH)){
			if(PathIsDirectory(buf))
				addDirectory(buf);
		}
	}

	DragFinish(drop);

	return 0;
}

LRESULT ShareDirectories::onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NM_LISTVIEW* lv = (NM_LISTVIEW*) pnmh;
	::EnableWindow(GetDlgItem(IDC_REMOVE_DIR), (lv->uNewState & LVIS_FOCUSED));
	::EnableWindow(GetDlgItem(IDC_RENAME_DIR), (lv->uNewState & LVIS_FOCUSED));

	NMITEMACTIVATE* itemActivate = (NMITEMACTIVATE*)pnmh;
	if((itemActivate->uChanged & LVIF_STATE) == 0)
		return 0;
	if((itemActivate->uOldState & INDEXTOSTATEIMAGEMASK(0xf)) == 0)
		return 0;
	if((itemActivate->uNewState & INDEXTOSTATEIMAGEMASK(0xf)) == 0)
		return 0;

	/* The checkbox state has changed */
	if(itemActivate->iItem >= 0) {
		auto sdi = (ShareDirInfo*)ctrlDirectories.GetItemData(itemActivate->iItem);
		bool boxChecked = ctrlDirectories.GetCheckState(itemActivate->iItem) > 0;

		if (sdi->incoming == boxChecked)
			return 0; //we are probably loading an initial listing...

		auto dirItems = getItemsByPath(sdi->path, true);
		for(auto& sdi: dirItems)
			sdi->incoming = boxChecked;

		auto s = find(newDirs.begin(), newDirs.end(), sdi);
		if (s == newDirs.end()) {
			auto s = find(changedDirs.begin(), changedDirs.end(), sdi);
			if (s == changedDirs.end()) {
				changedDirs.push_back(sdi);
			}
		}
	}

	return 0;		
}

ShareDirInfo::List ShareDirectories::getItemsByPath(const string& aPath, bool listRemoved) {
	ShareDirInfo::List dirItems;

	//add shared dirs
	for(const auto& d: shareDirs | map_values) {
		auto p = boost::find_if(d, [&aPath](const ShareDirInfoPtr& sdi) { return sdi->path == aPath; });
		if (p != d.end() && (listRemoved || find(removeDirs.begin(), removeDirs.end(), *p) == removeDirs.end()))
			dirItems.push_back(*p);
	}

	//add newly added dirs
	for(auto j = newDirs.begin(); j != newDirs.end(); j++) {
		if ((*j)->path == aPath) {
			dirItems.push_back(*j);
		}
	}

	return dirItems;
}

ShareDirectories::~ShareDirectories() { }

LRESULT ShareDirectories::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	//use the gui message for cleanup, these wont exist if the page hasnt been opened.
	if (ft.get()) {
		ft->UnsubclassWindow(true);
	}
	ctrlDirectories.DeleteAllItems();
	ctrlDirectories.Detach();
	return 0;
}

LRESULT ShareDirectories::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled) {
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch(kd->wVKey) {
	case VK_INSERT:
		PostMessage(WM_COMMAND, IDC_ADD_DIR, 0);
		break;
	case VK_DELETE:
		PostMessage(WM_COMMAND, IDC_REMOVE_DIR, 0);
		break;
	default:
		bHandled = FALSE;
	}
	return 0;
}

LRESULT ShareDirectories::onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/) {
	NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;

	if(item->iItem >= 0) {
		PostMessage(WM_COMMAND, IDC_RENAME_DIR, 0);
	} else if(item->iItem == -1) {
		PostMessage(WM_COMMAND, IDC_ADD_DIR, 0);
	}

	return 0;
}

void ShareDirectories::onCopyProfile(ProfileToken aNewProfile) {
	/* Add all items from the current profile */
	auto dirs = getViewItems(curProfile);
	for(const auto& sourceDir: dirs) {
		auto sdi = new ShareDirInfo(sourceDir->vname, aNewProfile, sourceDir->path);
		sdi->incoming = sourceDir->incoming;
		sdi->diffState = sourceDir->diffState;
		sdi->size = sourceDir->size;
		sdi->state = ShareDirInfo::STATE_ADDED;
		newDirs.push_back(sdi);
	}
}

void ShareDirectories::onRemoveProfile() {
	/* Newly added dirs can be just deleted... */
	for (auto i = newDirs.begin(); i != newDirs.end(); ) {
		if((*i)->profile == curProfile) {
			//delete *i;  
			i = newDirs.erase(i);
		} else {
			i++;
		}
	}

	/* Undo all current directory modifications to this profile */
	auto hasProfile = [this](const ShareDirInfoPtr& sdi) -> bool {
		return sdi->profile == curProfile;
	};

	removeDirs.erase(boost::remove_if(removeDirs, hasProfile), removeDirs.end());
	changedDirs.erase(boost::remove_if(changedDirs, hasProfile), changedDirs.end());

	excludedAdd.erase(boost::remove_if(excludedAdd, CompareFirst<ProfileToken, string>(curProfile)), excludedAdd.end());
	excludedRemove.erase(boost::remove_if(excludedRemove, CompareFirst<ProfileToken, string>(curProfile)), excludedRemove.end());


	/* Add all dirs in this profile to the list of removed dirs */
	auto& profileDirs = shareDirs[curProfile];
	removeDirs.insert(removeDirs.end(), profileDirs.begin(), profileDirs.end());

	/* List excludes */
	StringList excluded;
	ShareManager::getInstance()->getExcludes(curProfile, excluded);
	for(auto& path: excluded)
		excludedRemove.emplace_back(curProfile, path);
}

LRESULT ShareDirectories::onClickedAddDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	//add selected merged items missing from this profile
	int i = -1;
	bool added = false;
	while((i = ctrlDirectories.GetNextItem(i, LVNI_SELECTED)) != -1) {
		auto sdi = (ShareDirInfo*)ctrlDirectories.GetItemData(i);
		if (sdi->diffState == ShareDirInfo::DIFF_REMOVED) {
			sdi->diffState = ShareDirInfo::DIFF_NORMAL;
			sdi->state = ShareDirInfo::STATE_ADDED;
			newDirs.push_back(sdi);
			tempViewItems.erase(remove(tempViewItems.begin(), tempViewItems.end(), sdi), tempViewItems.end());
			added = true;
		}
	}

	if (!added) {
		tstring target;
		if(WinUtil::browseDirectory(target, m_hWnd)) {
			addDirectory(target);
		}
	} else if (SETTING(USE_OLD_SHARING_UI)) {
		//update the list to fix the colors and sorting
		RedrawWindow();
		ctrlDirectories.resort();
	}
	
	return 0;
}

LRESULT ShareDirectories::onClickedRemoveDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	bool redraw = false;
	int8_t confirmOption = CONFIRM_ASK;
	int selectedDirs = ctrlDirectories.GetSelectedCount();
	while((i = ctrlDirectories.GetNextItem(i, LVNI_SELECTED)) != -1) {
		auto sdi = (ShareDirInfo*)ctrlDirectories.GetItemData(i);
		if (sdi->diffState != ShareDirInfo::DIFF_REMOVED) {
			selectedDirs--;

			//the dir info gets deleted if this is a newly added item, just make sure that there's a reference by getting the items
			auto tmpItems = getViewItems(curProfile, false);

			removeDir(sdi->path, curProfile, confirmOption, true, selectedDirs);
			if (sdi->diffState == ShareDirInfo::DIFF_NORMAL && curProfile != SP_DEFAULT) {
				sdi->diffState = ShareDirInfo::DIFF_REMOVED;
				redraw = true;
			} else {
				ctrlDirectories.DeleteItem(i);
				i--;
			}
		}
	}

	if (redraw) {
		//update the list to fix the colors and sorting
		RedrawWindow();
		ctrlDirectories.resort();
	}
	
	return 0;
}

void ShareDirectories::removeDir(const string& rPath, ProfileToken aProfile, int8_t& confirmOption, bool checkDupes /*true*/, int remainingItems /*1*/) {
	//update the diff info in case this path exists in other profiles
	if (aProfile == SP_DEFAULT) {
		auto items = getItemsByPath(rPath, true);
		for (const auto& sdi: items) {
			if (sdi->profile != 0)
				sdi->diffState = ShareDirInfo::DIFF_REMOVED;
		}
	}

	//erase this dir (from newly added or shared)
	auto p = boost::find_if(newDirs, [&rPath, aProfile](const ShareDirInfoPtr& sdi) { return sdi->path == rPath && sdi->profile == aProfile; });
	if (p != newDirs.end()) {
		newDirs.erase(p);
	} else {
		auto& dirs = shareDirs[aProfile];
		auto k = boost::find_if(dirs, [&rPath, aProfile](const ShareDirInfoPtr& sdi) { return sdi->path == rPath && sdi->profile == aProfile; });
		if (k != dirs.end())
			removeDirs.push_back(*k);
	}

	if (checkDupes) {
		//check if this item exists in other profiles
		auto dirItems = getItemsByPath(rPath, false);
		if (!dirItems.empty() && Util::getOsMajor() >= 6) {
			if (confirmOption == CONFIRM_ASK) {
				CTaskDialog taskdlg;

				int sel;
				BOOL remember = FALSE;
				tstring msg = TSTRING_F(X_PROFILE_DIRS_EXISTS, (int)dirItems.size());
				taskdlg.SetMainInstructionText(msg.c_str());
				TASKDIALOG_BUTTON buttons[] =
				{
					{ IDC_REMOVE_OTHER, CTSTRING(REMOVE_OTHER_PROFILES), },
					{ IDC_LEAVE_OTHER, CTSTRING(LEAVE_OTHER_PROFILES), },
				};
				taskdlg.ModifyFlags(0, TDF_USE_COMMAND_LINKS);

				auto title = Text::toT(rPath);
				taskdlg.SetWindowTitle(title.c_str());
				//taskdlg.SetCommonButtons(TDCBF_CANCEL_BUTTON);

				taskdlg.SetButtons(buttons, _countof(buttons));
				taskdlg.SetMainIcon(TD_INFORMATION_ICON);

				tstring profiles = Text::toT(STRING(OTHER_PROFILES) + ":");
				for (auto i = dirItems.begin(); i != dirItems.end(); ++i)
					profiles += _T("\n") + Text::toT(parent->getProfile((*i)->profile)->getPlainName());

				taskdlg.SetExpandedInformationText(profiles.c_str());
				taskdlg.SetExpandedControlText(TSTRING(SHOW_OTHER_PROFILES).c_str());

				if (remainingItems >= 1) {
					tstring remove_msg = CTSTRING_F(PERFORM_X_REMAINING, remainingItems);
					taskdlg.SetVerificationText(remove_msg.c_str());
					taskdlg.DoModal(NULL, &sel, 0, &remember);
				} else {
					taskdlg.DoModal(NULL, &sel);
				}

				if (remember) {
					confirmOption = sel == IDC_REMOVE_OTHER ? CONFIRM_REMOVE : CONFIRM_LEAVE;
				} 
					
				if (sel == IDC_LEAVE_OTHER)
					return;
			} else if (confirmOption == CONFIRM_LEAVE) {
				//the remember option was used with a previous item
				return;
			}

			for (auto& sdi: dirItems) 
				removeDir(sdi->path, sdi->profile, confirmOption, false);
		}
	}
}

bool ShareDirectories::showShareDlg(const ShareProfile::List& spList, ProfileToken /*curProfile*/, const tstring& curName, ProfileTokenList& profilesTokens, tstring& newName, bool rename) {
	if (!spList.empty()) {
		SharePageDlg virt;
		virt.rename = rename;
		virt.line = curName;
		virt.profiles = spList;
		if(virt.DoModal(m_hWnd) == IDOK) {
			newName = virt.line;
			profilesTokens.insert(profilesTokens.end(), virt.selectedProfiles.begin(), virt.selectedProfiles.end());
		} else {
			return false;
		}
	} else {
		LineDlg virt;
		virt.title = TSTRING(VIRTUAL_NAME);
		virt.description = TSTRING(VIRTUAL_NAME_LONG);
		virt.line = curName;
		if(virt.DoModal(m_hWnd) != IDOK) {
			return false;
		}

		newName = virt.line;
	}
	newName = Text::toT(ShareManager::getInstance()->validateVirtual(Text::fromT(newName)));
	return true;
}

LRESULT ShareDirectories::onClickedRenameDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while((i = ctrlDirectories.GetNextItem(i, LVNI_SELECTED)) != -1) {
		auto sdi = (ShareDirInfo*)ctrlDirectories.GetItemData(i);
		if (sdi->diffState == ShareDirInfo::DIFF_REMOVED) {
			continue;
		}

		ProfileTokenList profilesTokens;
		profilesTokens.push_back(sdi->profile);

		tstring vName = Text::toT(sdi->vname);
		auto items = getItemsByPath(sdi->path, false);
		tstring newName;

		ShareProfile::List spList;
		for(auto j: items) {
			if (j->profile != curProfile)
				spList.push_back(parent->getProfile(j->profile));
		}

		if (!showShareDlg(spList, sdi->profile, vName, profilesTokens, newName, true))
			return 0;

		if (stricmp(vName, newName) != 0) {
			ctrlDirectories.SetItemText(i, 0, newName.c_str());

			for(auto j: profilesTokens) {
				auto s = find_if(items.begin(), items.end(), [j](const ShareDirInfoPtr& tmp) { return tmp->profile == j; });
				(*s)->vname = Text::fromT(newName);

				/* Is this a newly added dir? */
				auto p = find(newDirs.begin(), newDirs.end(), *s);
				if (p == newDirs.end() && find(changedDirs.begin(), changedDirs.end(), *s) == changedDirs.end()) {
					changedDirs.push_back(*s);
				}
			}
		} else {
			MessageBox(CTSTRING(SKIP_RENAME), _T(APPNAME) _T(" ") _T(VERSIONSTRING), MB_ICONINFORMATION | MB_OK);
		}
	}

	return 0;
}

bool ShareDirectories::addDirectory(const tstring& aPath){
	tstring path = aPath;
	if(path[path.length()-1] != PATH_SEPARATOR)
		path += PATH_SEPARATOR;

	/* Check if we are trying to share a directory which exists already in this profile */
	auto dirs = getViewItems(curProfile);
	for(const auto& sdi: dirs) {
		if (AirUtil::isParentOrExact(sdi->path, Text::fromT(aPath))) {
			MessageBox(CTSTRING(DIRECTORY_SHARED), _T(APPNAME) _T(" ") _T(VERSIONSTRING), MB_ICONINFORMATION | MB_OK);
			return false;
		} else if (AirUtil::isSub(sdi->path, Text::fromT(aPath))) {
			if (ft.get()) {
				//Remove the subdir but only from this profile
				int8_t confirmOption = CONFIRM_LEAVE;
				removeDir(sdi->path, sdi->profile, confirmOption, false);
				//break;
			} else {
				//Maybe we should find and remove the correct item from the list instead...
				MessageBox(CTSTRING(DIRECTORY_PARENT), _T(APPNAME) _T(" ") _T(VERSIONSTRING), MB_ICONINFORMATION | MB_OK);
				return false;
			}
		}
	}

	ShareProfile::List spList;
	ProfileTokenList profileTokens;
	tstring newName;
	profileTokens.push_back(curProfile);

	//list the profiles that don't have this directory
	for(const auto& sp: parent->getProfiles()) {
		if (sp->getToken() == curProfile)
			continue;

		auto items = move(getViewItems(sp->getToken(), false));
		auto p = boost::find_if(items, [&path](const ShareDirInfoPtr& sdi) { return sdi->path == Text::fromT(path); });
		if (p == items.end()) {
			spList.push_back(sp);
		}
	}

	if(showShareDlg(spList, curProfile, Text::toT(ShareManager::getInstance()->validateVirtual(Util::getLastDir(Text::fromT(path)))), profileTokens, newName, false)) {
		string vPath = Text::fromT(newName), rPath = Text::fromT(path);
		try {
			ShareManager::getInstance()->validatePath(rPath, vPath);
		} catch(ShareException& e) { 
			MessageBox(Text::toT(e.getError()).c_str(), _T(APPNAME) _T(" ") _T(VERSIONSTRING), MB_OK | MB_ICONEXCLAMATION);
			return false;
		}

		for(const auto pt: profileTokens) {
			ShareDirInfoPtr dir = new ShareDirInfo(vPath, pt, rPath);
			if (find_if(newDirs.begin(), newDirs.end(), [dir](const ShareDirInfoPtr& sdi) { return compare(sdi->profile, dir->profile) == 0 && sdi->path == dir->path; }) != newDirs.end()) {
				//delete dir;
				continue;
			}

			//update the diff information
			auto items = getItemsByPath(rPath, true);
			if (pt == SP_DEFAULT) {
				//update the diff info in case this path exists in other profiles
				for (auto& sdi: items)
					sdi->diffState = ShareDirInfo::DIFF_NORMAL;
			} else {
				//check if this exists in the default profile (and the default profile isn't also being added now)
				auto pos = boost::find_if(items, [](const ShareDirInfoPtr& sdi) { return sdi->profile == SP_DEFAULT; });
				if (pos == items.end() && find(profileTokens.begin(), profileTokens.end(), SP_DEFAULT) == profileTokens.end())
					dir->diffState = ShareDirInfo::DIFF_ADDED;
			}

			auto p = find_if(removeDirs.begin(), removeDirs.end(), [dir](const ShareDirInfoPtr& sdi) { return compare(sdi->profile, dir->profile) == 0 && sdi->path == dir->path; });
			if (p != removeDirs.end()) {
				//check if the virtual name is different...
				if ((*p)->vname != dir->vname) {
					changedDirs.push_back(dir);
				} else {
					//delete dir;
					dir = *p;
				}
				removeDirs.erase(p);
			} else {
				dir->state = ShareDirInfo::STATE_ADDED;
				newDirs.push_back(dir);
			}

			if(SETTING(USE_OLD_SHARING_UI) && pt == curProfile) {
				int i = ctrlDirectories.insert(ctrlDirectories.GetItemCount(), Text::toT(dir->vname), 0, (LPARAM)dir.get());
				ctrlDirectories.SetItemText(i, 1, Text::toT(dir->path).c_str());
				ctrlDirectories.SetItemText(i, 2, dir->size == 0 ? _T("New") : Util::formatBytesW(dir->size).c_str());
			}
		}

		ctrlDirectories.resort();
	}
	return true;
}

void ShareDirectories::write() { }

Dispatcher::F ShareDirectories::getThreadedTask() {
	if (hasChanged()) {
		return Dispatcher::F([=] { 
			applyChanges(true); 
		});
	}

	return nullptr;
}

/*void ShareDirectories::fixControls() {
	//::EnableWindow(GetDlgItem(IDC_APPLY_CHANGES), hasChanged());
	::EnableWindow(GetDlgItem(IDC_REMOVE_PROFILE), curProfile != SP_DEFAULT);
}*/

bool ShareDirectories::hasChanged() {
	return (!newDirs.empty() || !removeDirs.empty() || !changedDirs.empty() || !excludedAdd.empty() || !excludedRemove.empty());
}

void ShareDirectories::applyChanges(bool /*isQuit*/) {
	if (!newDirs.empty()) {
		ShareManager::getInstance()->addDirectories(newDirs);
		//will be cleared later...
	}

	if (!removeDirs.empty()) {
		ShareManager::getInstance()->removeDirectories(removeDirs);
		//removeDirs.clear();
	}

	if (!changedDirs.empty()) {
		ShareManager::getInstance()->changeDirectories(changedDirs);
		//changedDirs.clear();
	}

	if (!excludedRemove.empty() || !excludedAdd.empty()) {
		ShareManager::getInstance()->changeExcludedDirs(excludedAdd, excludedRemove);
		//aExcludedAdd.clear();
		//aExcludedRemove.clear();
	}

	/*if (!isQuit) {
		deleteDirectoryInfoItems();
		ShareManager::getInstance()->getShares(shareDirs);
		showProfile();
		::EnableWindow(GetDlgItem(IDC_APPLY_CHANGES), false);
	}*/
}

void ShareDirectories::deleteTempViewItems() {
	//boost::for_each(tempViewItems, DeleteFunction());
	tempViewItems.clear();
}

bool ShareDirectories::addExcludeFolder(const string& path) {
	auto excludes = getExcludedDirs();
	auto shares = getViewItems(curProfile);
	
	// make sure this is a sub folder of a shared folder
	if (boost::find_if(shares, [&path](const ShareDirInfoPtr& aDir) { return AirUtil::isSub(path, aDir->path); } ) == shares.end())
		return false;

	// Make sure this not a subfolder of an already excluded folder
	if (boost::find_if(excludes, [&path](const string& aDir) { return AirUtil::isParentOrExact(aDir, path); } ) != excludes.end())
		return false;

	// remove all sub folder excludes
	for(auto& j: excludes) {
		if (AirUtil::isSub(j, path))
			removeExcludeFolder(j);
	}

	// add it to the list
	excludedAdd.emplace_back(curProfile, path);
	//fixControls();
	return true;
}

bool ShareDirectories::removeExcludeFolder(const string& path) {
	auto add = boost::find_if(excludedAdd, CompareSecond<ProfileToken, string>(path));
	if (add != excludedAdd.end()) {
		excludedAdd.erase(add);
		return true;
	}

	excludedRemove.emplace_back(curProfile, path);
	//fixControls();
	return true;
}

bool ShareDirectories::shareFolder(const string& path, ShareDirInfo::List& aShared) {
	auto excludes = getExcludedDirs();

	// check if it's part of the share before checking if it's in the exclusions
	bool result = false;
	ShareDirInfo::List::iterator i;
	for(i = aShared.begin(); i != aShared.end(); ++i) {
		if ((*i)->profile != curProfile)
			continue;

		if((path.size() == (*i)->path.size()) && (stricmp(path, (*i)->path) == 0)) {
			// is it a perfect match
			(*i)->found = true;
			return true;
		} else if (path.size() > (*i)->path.size()) // this might be a subfolder of a shared folder
		{
			// if the left-hand side matches and there is a \ in the remainder then it is a subfolder
			if((stricmp(path.substr(0, (*i)->path.size()), (*i)->path) == 0) && (path.find('\\', (*i)->path.size()) != string::npos)) {
				result = true;
				break;
			}
		}
	}

	if(!result)
		return false;

	// check if it's an excluded folder or a sub folder of an excluded folder
	if (boost::find_if(excludes, [&path](const string& aDir) { return AirUtil::isParentOrExact(aDir, path); } ) != excludes.end())
		return false;

	(*i)->found = true;
	return true;
}