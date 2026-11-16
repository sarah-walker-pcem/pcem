#ifndef _PCEM_WX_CONFIG_H_
#define _PCEM_WX_CONFIG_H_

#include "wx-utils.h"
#include "wx-hdgeometry.h"

#include <wx/choicebk.h>
#include <wx/listctrl.h>
#include <wx/simplebook.h>
#include <wx/spinctrl.h>

#include <vector>

#define NUM_DRIVES 7

struct device_t;

extern int has_been_inited;

class ConfigDialog : public wxDialog {
    public:
        static bool Open(wxWindow *parent);

        ConfigDialog(wxWindow *parent);

    private:
        struct DriveControls {
                wxWindow *panel;
                wxComboBox *type;
                wxWindow *new_button;
                wxWindow *file_button;
                wxWindow *eject_button;
                wxTextCtrl *fn;
                wxStaticText *spt;
                wxStaticText *hpc;
                wxStaticText *cyl;
                wxStaticText *size;
                wxWindow *labels[6];
        };

        static int MouseValid(int type, int model_index);
        static wxString NormalizeMacaddr(const wxString &text, bool *valid);

        std::vector<int> romstolist, listtomodel, romstomodel, modeltolist;
        std::vector<int> sound_to_list, list_to_sound;
        std::vector<int> mouse_to_list, list_to_mouse;
#ifdef USE_NETWORKING
        std::vector<int> network_to_list, list_to_network;
#endif
        std::vector<char *> hdd_names;
        std::vector<int> cd_list_to_model;

        bool hd_changed;
        int new_cdrom_channel;
        int new_zip_channel;
        int prev_model;

        wxComboBox *cb_model, *cb_cpu_manu, *cb_cpu, *cb_fpu, *cb_waitstates;
        wxCheckBox *chk_dynarec, *chk_sync;
        wxSpinCtrl *spin_mem;
        wxStaticText *text_mem_unit;
        wxComboBox *cb_video, *cb_video_speed;
        wxCheckBox *chk_voodoo;
        wxComboBox *cb_sound;
        wxCheckBox *chk_gameblaster, *chk_gus, *chk_ssi;
        wxComboBox *cb_hdd, *cb_fda, *cb_fdb, *cb_cd_model, *cb_cd_speed;
        wxChoicebook *hd_book;
        wxComboBox *cb_lpt1, *cb_mouse, *cb_joystick;
        wxWindow *btn_joystick[4];
        wxWindow *btn_model_cfg, *btn_video_cfg, *btn_voodoo_cfg, *btn_sound_cfg, *btn_hdd_cfg, *btn_lpt1_cfg;
        wxComboBox *cb_netcard;
        wxWindow *btn_netcard_cfg, *text_macaddr, *net_panel;
        wxTextCtrl *edit_macaddr;
        wxListCtrl *page_list;
        wxSimplebook *pages;
        DriveControls drives[NUM_DRIVES];

        template <typename T> T *Ctrl(const wxString &name) { return wx_ctrl<T>(this, name); }

        template <typename T> T *Ctrl(const char *name, int index) {
                return wx_ctrl<T>(this, wxString::Format("%s[%i]", name, index));
        }

        void FindControls();
        int SelectedModel();
        int SelectedIndex(wxComboBox *cb);
        char *SelectedHdd();
        bool SelectedHddIsMfm();
        bool SelectedHddIsIde();
        bool SelectedHddIsScsi();
        bool SelectedHddHasConfig();
        device_t *SelectedHddDevice();
        int SelectedCdModel();
        void RecalcFpuList(int model_index, int manu, int cpu_index, int fpu);
        void RecalcVidList(int model_index, bool force_builtin_video);
        void RecalcSndList(int model_index);
        void RecalcHddList(int model_index, bool use_selected_hdd, bool force_ide);
        void RecalcCdList(int cur_speed, const char *cur_model);
        void RecalcCdListFromSelection();
        void PopulateMouseList(int model_index, int select_type);
        void PopulateCpuManufacturers(int model_index, int selection);
        void PopulateCpus(int model_index, int manu, int selection);
        void UpdateCpuDependent(int model_index, int manu, int cpu_index, int fpu);
        int SelectedFpuType(int model_index, int manu, int cpu_index);
        void UpdateMemoryUnit(int model_index);
        void UpdateJoystickButtons(int type);

#ifdef USE_NETWORKING
        void RecalcNetList(int model_index);
        int SelectedNetworkCard();
        bool SelectedNetworkCardHasDevice();
        void UpdateMacaddrVisibility();
        bool ValidateMacaddr();
#endif

        void InitControls();
        void BindEvents();
        int SelectedVideoCard();
        void OnPageSelected(wxListEvent &event);
        void OnModelChanged(wxCommandEvent &event);
        void OnCpuManufacturerChanged(wxCommandEvent &event);
        void OnCpuChanged(wxCommandEvent &event);
        void OnVideoChanged(wxCommandEvent &event);
        void OnHddChanged(wxCommandEvent &event);
        void SetDriveGeometry(int drive, const HdGeometryDialog::HdGeometry &g, const wxString &fn);
        void HdConfInit();
        void HdConfUpdate();
        void UpdateHddCdrom();
        void HdEject(int drive);
        void HdNew(int drive);
        void CheckHdType(off_t sz, HdGeometryDialog::HdGeometry &g);
        void HdFile(int drive);
        void HdDriveType(int drive);
        void Save();
        void OnOk(wxCommandEvent &event);
        void OnCancel(wxCommandEvent &event);
};

#endif /* _PCEM_WX_CONFIG_H_ */
