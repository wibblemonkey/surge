//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "sub3_editor.h"
#include "resource.h"
#include "gui_slider.h"
#include "gui_hswitch.h"
#include "gui_switch.h"
#include "gui_pdisplay.h"
#include "gui_patchname.h"
#include "gui_wavedisplay.h"
#include "gui_modsrcbutton.h"
#include "CSnapshotMenu.h"
#include "gui_superlfo.h"
#include "gui_fxconf.h"
#include "gui_vumeter.h"
#include "gui_fxlabel.h"
#include "CAboutBox.h"
#include "vstcontrols.h"
#include "surge_bitmap_keeper.h"
#include "cparamedit.h"

/*#if TARGET_AUDIOUNIT
#include "aulayer.h"
#elif TARGET_VST3
#include "surgeprocessor.h"
#elif TARGET_VST2
#include "vstlayer.h"
#endif*/

//#include <commctrl.h>
const int window_size_x = 904, window_size_y = 542;
const int yofs = 10;

#ifdef MAC
SharedPointer<CFontDesc> minifont = new CFontDesc("Lucida Grande", 9);
SharedPointer<CFontDesc> patchfont = new CFontDesc("Lucida Grande", 14);
#else
SharedPointer<CFontDesc> minifont = new CFontDesc("Microsoft Sans Serif", 9);
SharedPointer<CFontDesc> patchfont = new CFontDesc("Arial", 14);
#endif

CFontRef surge_minifont = minifont;
CFontRef surge_patchfont = patchfont;

enum special_tags
{
   tag_scene_select = 1,
   tag_osc_select,
   tag_osc_menu,
   tag_fx_select,
   tag_fx_menu,
   tag_patchname,
   tag_mp_category,
   tag_mp_patch,
   tag_store,
   tag_store_cancel,
   tag_store_ok,
   tag_store_name,
   tag_store_category,
   tag_store_creator,
   tag_store_comments,
   tag_about,
   tag_mod_source0,
   tag_mod_source_end = tag_mod_source0 + n_modsources,
   //	tag_metaparam,
   // tag_metaparam_end = tag_metaparam+n_customcontrollers,
   start_paramtags,
};

sub3_editor::sub3_editor(void* effect, sub3_synth* synth) : super(effect)
{
   frame = 0;

#if TARGET_VST3
   // setIdleRate(25);
   // synth = ((SurgeProcessor*)effect)->getSurge();
#endif

   patchname = 0;
   current_scene = 1;
   current_osc = 0;
   current_fx = 0;
   modsource = ms_lfo1;
   modsource_editor = ms_lfo1;
   blinktimer = 0.f;
   blinkstate = false;
   aboutbox = 0;

   mod_editor = false;

   // init the size of the plugin
   rect.left = 0;
   rect.top = 0;
   rect.right = window_size_x;
   rect.bottom = window_size_y;
   editor_open = false;
   queue_refresh = false;
   memset(param, 0, 1024 * sizeof(void*));
   polydisp = 0;
   clear_infoview_countdown = -1;
   vu[0] = 0;
   vu[1] = 0;
   vu[2] = 0;
   vu[3] = 0;
   vu[4] = 0;
   vu[5] = 0;
   vu[6] = 0;
   vu[7] = 0;
   vu[8] = 0;
   vu[9] = 0;
   vu[10] = 0;
   vu[11] = 0;
   vu[12] = 0;
   vu[13] = 0;
   vu[14] = 0;
   vu[15] = 0;
   lfodisplay = 0;
   idleinc = 0;

   _effect = effect;
   this->synth = synth;

   // ToolTipWnd = 0;
}

sub3_editor::~sub3_editor()
{
   if (frame)
   {
      getFrame()->unregisterKeyboardHook(this);
      frame->removeAll(true);
   }
}

void sub3_editor::idle()
{
   if (!synth)
      return;
   if (editor_open && frame && !synth->halt_engine)
   {
      if (aboutbox && (aboutbox->getValue() > 0.5f))
         return;
      /*static CDrawContext drawContext
      (frame, NULL, systemWindow);*/
      // CDrawContext *drawContext = frame->createDrawContext();

      CView* v = frame->getFocusView();
      if (v && dynamic_cast<CControl*>(v) != nullptr)
      {
         int ptag = ((CControl*)v)->getTag() - start_paramtags;
         if (ptag >= 0)
         {
            synth->storage.CS_ModRouting.enter();
            for (int i = 1; i < n_modsources; i++)
            {
               ((gui_modsrcbutton*)gui_modsrc[i])
                   ->update_rt_vals(synth->isActiveModulation(ptag, i), 0,
                                    synth->isModsourceUsed(i));
            }
            synth->storage.CS_ModRouting.leave();
         }
      }
      else
      {
         synth->storage.CS_ModRouting.enter();
         for (int i = 1; i < n_modsources; i++)
         {
            ((gui_modsrcbutton*)gui_modsrc[i])->update_rt_vals(false, 0, synth->isModsourceUsed(i));
         }
         synth->storage.CS_ModRouting.leave();
      }
#if MAC
      idleinc++;
      if (idleinc > 15)
      {
         idleinc = 0;
#else
      SYSTEMTIME st;
      GetSystemTime(&st);

      if (((st.wMilliseconds > 500) && blinkstate) || ((st.wMilliseconds <= 500) && !blinkstate))
      {
#endif
         for (int i = 1; i < n_modsources; i++)
         {
            ((gui_modsrcbutton*)gui_modsrc[i])->setblink(blinkstate);
         }
         blinkstate = !blinkstate;
      }

      if (synth->storage.getPatch().scene[current_scene].osc[current_osc].wt.refresh_display)
      {
         synth->storage.getPatch().scene[current_scene].osc[current_osc].wt.refresh_display = false;
         if (oscdisplay)
         {
            oscdisplay->setDirty(true);
            oscdisplay->invalid();
         }
      }

      if (polydisp)
      {
         ((CParamEdit*)polydisp)->setPoly(synth->polydisplay);
      }

      if (queue_refresh || synth->refresh_editor)
      {
         queue_refresh = false;
         synth->refresh_editor = false;

         if (frame)
         {
            if (synth->patch_loaded)
               mod_editor = false;
            synth->patch_loaded = false;

            open_editor();
         }
         if (patchname)
         {
            ((gui_patchname*)patchname)->setLabel(synth->storage.getPatch().name);
            ((gui_patchname*)patchname)->setCategory(synth->storage.getPatch().category);
            ((gui_patchname*)patchname)->setAuthor(synth->storage.getPatch().author);
         }
      }

      vu[0]->setValue(synth->vu_peak[0]);
      ((gui_vumeter*)vu[0])->setValueR(synth->vu_peak[1]);
      vu[0]->invalid();
      for (int i = 0; i < 8; i++)
      {
         assert(i + 1 < baseeffect::KNumVuSlots);
         if (vu[i + 1] && synth->fx[current_fx])
         {
            // there's seems to be a bug here that overwrites either this or the vu-pointer
            // try to catch it earlier to retrieve more info

            // assert(!((int)vu[i+1] & 0xffff0000));

            // check so it doesn't overlap with the infowindow
            CRect iw = ((gui_pdisplay*)infowindow)->getViewSize();
            CRect vur = vu[i + 1]->getViewSize();

            if (!((gui_pdisplay*)infowindow)->isVisible() || !vur.rectOverlap(iw))
            {
               vu[i + 1]->setValue(synth->fx[current_fx]->vu[(i << 1)]);
               ((gui_vumeter*)vu[i + 1])->setValueR(synth->fx[current_fx]->vu[(i << 1) + 1]);
               vu[i + 1]->invalid();
            }
         }
      }

      for (int i = 0; i < 8; i++)
      {
         if (synth->refresh_ctrl_queue[i] >= 0)
         {
            int j = synth->refresh_ctrl_queue[i];
            synth->refresh_ctrl_queue[i] = -1;

            if (param[j])
            {
               char pname[256], pdisp[256], txt[256];
               synth->getParameterName(j, pname);
               synth->getParameterDisplay(j, pdisp);

               /*if(i == 0)
               {
                       ((gui_pdisplay*)infowindow)->setLabel(pname,pdisp);
                       draw_infowindow(j, param[j], false, true);
                       clear_infoview_countdown = 40;
               }*/

               param[j]->setValue(synth->refresh_ctrl_queue_value[i]);
               frame->invalidRect(param[j]->getViewSize());
               // oscdisplay->invalid();
            }
         }
      }
      for (int i = 0; i < 8; i++)
      {
         if (synth->refresh_parameter_queue[i] >= 0)
         {
            int j = synth->refresh_parameter_queue[i];
            synth->refresh_parameter_queue[i] = -1;
            if ((j < n_total_params) && param[j])
            {
               param[j]->setValue(synth->getParameter01(j));
               frame->invalidRect(param[j]->getViewSize());
            }
            else if ((j >= metaparam_offset) && (j < (metaparam_offset + n_customcontrollers)))
            {
               int cc = j - metaparam_offset;
               gui_modsrc[ms_ctrl1 + cc]->setValue(
                   ((modulation_controller*)synth->storage.getPatch().scene[0].modsources[ms_ctrl1 +
                                                                                          i])
                       ->get_target01());
            }
         }
      }
      for (int i = 0; i < n_customcontrollers; i++)
      {
         if (((modulation_controller*)synth->storage.getPatch().scene[0].modsources[ms_ctrl1 + i])
                 ->has_changed(true))
         {
            gui_modsrc[ms_ctrl1 + i]->setValue(((modulation_controller*)synth->storage.getPatch()
                                                    .scene[0]
                                                    .modsources[ms_ctrl1 + i])
                                                   ->get_target01());
         }
      }
      clear_infoview_countdown--;
      if (clear_infoview_countdown == 0)
      {
         ((gui_pdisplay*)infowindow)->Hide();
         // infowindow->getViewSize();
         // ctnvg			frame->redrawRect(drawContext,r);
      }
      // frame->update(&drawContext);
      // frame->idle();
   }
}

void sub3_editor::toggle_mod_editing()
{
   mod_editor = !mod_editor;
   refresh_mod();
}

void sub3_editor::refresh_mod()
{
   synth->storage.CS_ModRouting.enter();
   for (int i = 0; i < 512; i++)
   {
      if (param[i])
      {
         gui_slider* s = (gui_slider*)param[i];
         if (s->is_mod)
         {
            s->setModMode(mod_editor ? 1 : 0);
            s->setModPresent(synth->isModDestUsed(i));
            s->setModCurrent(synth->isActiveModulation(i, modsource));
         }
         // s->setDirty();
         s->setModValue(synth->getModulation(i, modsource));
         s->invalid();
      }
   }
   synth->storage.CS_ModRouting.leave();
   for (int i = 1; i < n_modsources; i++)
   {
      int state = 0;
      if (i == modsource)
         state = mod_editor ? 2 : 1;
      if (i == modsource_editor)
         state |= 4;
      ((gui_modsrcbutton*)gui_modsrc[i])->state = state;
      ((gui_modsrcbutton*)gui_modsrc[i])->invalid();
   }

   // ctnvg	frame->redraw();
   // frame->setDirty();
}

int32_t sub3_editor::onKeyDown(const VstKeyCode& code, CFrame* frame)
{
   switch (code.character)
   {
   case VKEY_ALT:
      // modsource = modsource_editor;
      // queue_refresh = true;
      mod_editor = true;
      refresh_mod();
      return 1;
   case VKEY_TAB:
      toggle_mod_editing();
      return 1;
   case VKEY_LEFT:
      synth->increment_patch(-1, 0);
      return 1;
   case VKEY_RIGHT:
      synth->increment_patch(1, 0);
      return 1;
   case VKEY_UP:
      synth->increment_patch(0, -1);
      return 1;
   case VKEY_DOWN:
      synth->increment_patch(0, 1);
      return 1;
   }
   return -1;
}

int32_t sub3_editor::onKeyUp(const VstKeyCode& keyCode, CFrame* frame)
{
   switch (keyCode.character)
   {
   case VKEY_ALT:
      // modsource = 0;
      // queue_refresh = true;
      mod_editor = false;
      refresh_mod();
      return 1;
   }
   return -1;
}

bool sub3_editor::is_visible(int subsec, int subsec_id)
{
   switch (subsec)
   {
   case 2: // osc section, uses switching
      return (subsec_id == current_osc);
   case 6:
      return (subsec_id == modsource_editor);
   case 7:
      return (subsec_id == current_fx);
   }
   return true; // visible by default
}

CRect positionForModulationGrid(int entry)
{
   const int width = is_customcontroller(entry) ? 75 : 64;
   CRect r(2, 1, width, 14 + 1);

   if (is_customcontroller(entry))
      r.bottom += 8;
   int gridX = modsource_grid_xy[entry][0];
   int gridY = modsource_grid_xy[entry][1];
   r.offset(0, 399 + 8 * gridY);
   for (int i = 0; i < gridX; i++)
   {
      r.offset((i >= 7) ? 75 : 64, 0);
   }

   return r;
}

void sub3_editor::open_editor()
{
   if (!synth)
      return;
   assert(frame);

   getFrame()->registerKeyboardHook(this);

   if (editor_open)
      close_editor();

   CPoint nopoint(0, 0);

   current_scene = synth->storage.getPatch().scene_active.val.i;

   {
      CRect rect(0, 0, 75, 13);
      rect.offset(104 - 36, 69);
      CControl* oscswitch = new gui_hswitch(rect, this, tag_osc_select, 3, 13, 1, 3,
                                            getSurgeBitmap(IDB_OSCSELECT), nopoint);
      oscswitch->setValue((float)current_osc / 2.0f);
      frame->addView(oscswitch);
   }

   {
      CRect rect(0, 0, 119, 51);
      rect.offset(764 + 3, 71);
      gui_fxconf* fc = new gui_fxconf(rect, this, tag_fx_select, current_fx);
      ccfxconf = fc;
      for (int i = 0; i < 8; i++)
      {
         fc->set_type(i, synth->storage.getPatch().fx[i].type.val.i);
      }
      fc->set_bypass(synth->storage.getPatch().fx_bypass.val.i);
      fc->set_disable(synth->storage.getPatch().fx_disable.val.i);
      frame->addView(fc);
   }

   int rws = 15;
   for (int k = 1; k < n_modsources; k++)
   {
      CRect r = positionForModulationGrid(k);

      int state = 0;
      if (k == modsource)
         state = mod_editor ? 2 : 1;
      if (k == modsource_editor)
         state |= 4;

      gui_modsrc[k] = new gui_modsrcbutton(r, this, tag_mod_source0 + k, state, k);
      ((gui_modsrcbutton*)gui_modsrc[k])->update_rt_vals(false, 0, synth->isModsourceUsed(k));
      if ((k >= ms_ctrl1) && (k <= ms_ctrl8))
      {
         ((gui_modsrcbutton*)gui_modsrc[k])
             ->setlabel(synth->storage.getPatch().CustomControllerLabel[k - ms_ctrl1]);
         ((gui_modsrcbutton*)gui_modsrc[k])->set_ismeta(true);
         ((gui_modsrcbutton*)gui_modsrc[k])
             ->setBipolar(synth->storage.getPatch().scene[0].modsources[k]->is_bipolar());
         gui_modsrc[k]->setValue(
             ((modulation_controller*)synth->storage.getPatch().scene[0].modsources[k])
                 ->get_target01());
      }
      else
      {
         ((gui_modsrcbutton*)gui_modsrc[k])->setlabel(modsource_abberations_button[k]);
      }
      frame->addView(gui_modsrc[k]);
   }

   /*// Comments
   {
           CRect CommentsRect(6 + 150*4,528, window_size_x, window_size_y);
           CTextLabel *Comments = new
   CTextLabel(CommentsRect,synth->storage.getPatch().comment.c_str());
           Comments->setTransparency(true);
           Comments->setFont(surge_minifont);
           Comments->setHoriAlign(kMultiLineCenterText);
           frame->addView(Comments);
   }*/

   // main vu-meter
   CRect vurect(763, 0, 763 + 123, 13);
   vurect.offset(0, 14);
   vu[0] = new gui_vumeter(vurect);
   ((gui_vumeter*)vu[0])->setType(vut_vu_stereo);
   frame->addView(vu[0]);

   // fx vu-meters & labels

   vurect.offset(0, 162 + 8);
   if (synth->fx[current_fx])
   {
      for (int i = 0; i < 8; i++)
      {
         int t = synth->fx[current_fx]->vu_type(i);
         if (t)
         {
            CRect vr(vurect);
            vr.offset(0, yofs * synth->fx[current_fx]->vu_ypos(i));
            vr.offset(0, 7);
            vu[i + 1] = new gui_vumeter(vr);
            ((gui_vumeter*)vu[i + 1])->setType(t);
            frame->addView(vu[i + 1]);
         }
         else
            vu[i + 1] = 0;

         const char* label = synth->fx[current_fx]->group_label(i);

         if (label)
         {
            CRect vr(vurect);
            vr.top += 1;
            vr.offset(0, 9);
            vr.offset(0, yofs * synth->fx[current_fx]->group_label_ypos(i));
            gui_fxlabel* lb = new gui_fxlabel(vr);
            lb->setLabel(label);
            frame->addView(lb);
         }
      }
   }

   // CRect(12,62,140,159)
   oscdisplay = new gui_wavedisplay(CRect(6, 81, 142, 180),
                                    &synth->storage.getPatch()
                                         .scene[synth->storage.getPatch().scene_active.val.i]
                                         .osc[current_osc],
                                    &synth->storage);
   frame->addView(oscdisplay);

   // 150*b - 16 = 434 (b=3)
   patchname =
       new gui_patchname(CRect(156, 11, 591, 11 + 28), this, tag_patchname, &synth->storage);
   ((gui_patchname*)patchname)->setLabel(synth->storage.getPatch().name);
   ((gui_patchname*)patchname)->setCategory(synth->storage.getPatch().category);
   ((gui_patchname*)patchname)->setIDs(synth->current_category_id, synth->patchid);
   ((gui_patchname*)patchname)->setAuthor(synth->storage.getPatch().author);
   frame->addView(patchname);

   gui_hswitch* mp_cat =
       new gui_hswitch(CRect(157, 41, 157 + 37, 41 + 12), this, tag_mp_category, 2, 12, 1, 2,
                       getSurgeBitmap(IDB_BUTTON_MINUSPLUS), nopoint, false);
   frame->addView(mp_cat);

   gui_hswitch* mp_patch =
       new gui_hswitch(CRect(242, 41, 242 + 37, 41 + 12), this, tag_mp_patch, 2, 12, 1, 2,
                       getSurgeBitmap(IDB_BUTTON_MINUSPLUS), nopoint, false);
   frame->addView(mp_patch);

   gui_hswitch* b_store = new gui_hswitch(CRect(591 - 37, 41, 591, 41 + 12), this, tag_store, 1, 12,
                                          1, 1, getSurgeBitmap(IDB_BUTTON_STORE), nopoint, false);
   frame->addView(b_store);

   memset(param, 0, 512 * sizeof(void*));
   int i = 0;
   vector<parameter*>::iterator iter;
   for (iter = synth->storage.getPatch().param_ptr.begin();
        iter != synth->storage.getPatch().param_ptr.end(); iter++)
   {
      parameter* p = *iter;

      if ((p->posx != 0) && ((p->scene == (current_scene + 1)) || (p->scene == 0)) &&
          is_visible(p->ctrlgroup, p->ctrlgroup_entry) && (p->ctrltype != ct_none))
      {
         long style = p->ctrlstyle;
         /*if(p->ctrlstyle == cs_hori) style = kHorizontal;
         else if(p->ctrlstyle == cs_vert) style = kVertical | kBottom;*/
         switch (p->ctrltype)
         {
         case ct_decibel:
         case ct_decibel_narrow:
         case ct_decibel_extra_narrow:
         case ct_freq_mod:
         case ct_percent_bidirectional:
         case ct_freq_shift:
            style |= kBipolar;
            break;
         };

         switch (p->ctrltype)
         {
         case ct_filtertype:
         {
            CRect rect(0, 0, 129, 18);
            rect.offset(p->posx - 2, p->posy + 1);
            CControl* hsw = new gui_hswitch(rect, this, p->id + start_paramtags, 10, 18, 1, 10,
                                            getSurgeBitmap(IDB_FILTERBUTTONS), nopoint, true);
            rect(3, 0, 124, 14);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_filtersubtype:
         {
            CRect rect(0, 0, 12, 18);
            rect.offset(p->posx + 129, p->posy + 1);
            CControl* hsw = new gui_switch(rect, this, p->id + start_paramtags,
                                           getSurgeBitmap(IDB_FILTERSUBTYPE));
            rect(1, 1, 9, 14);
            ((gui_switch*)hsw)->is_itype = true;
            ((gui_switch*)hsw)->imax = 3;
            ((gui_switch*)hsw)->ivalue = p->val.i + 1;
            if (fut_subcount[synth->storage.getPatch()
                                 .scene[current_scene]
                                 .filterunit[p->ctrlgroup_entry]
                                 .type.val.i] == 0)
               ((gui_switch*)hsw)->ivalue = 0;
            rect.offset(p->posx + 129, p->posy + 1);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);

            if (p->ctrlgroup_entry == 1)
            {
               f2subtypetag = p->id + start_paramtags;
               filtersubtype[1] = hsw;
            }
            else
            {
               f1subtypetag = p->id + start_paramtags;
               filtersubtype[0] = hsw;
            }
         }
         break;
         case ct_bool_keytrack:
         {
            CRect rect(0, 0, 43, 7);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new gui_switch(rect, this, p->id + start_paramtags,
                                           getSurgeBitmap(IDB_SWITCH_KTRK));
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_bool_retrigger:
         {
            CRect rect(0, 0, 43, 7);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new gui_switch(rect, this, p->id + start_paramtags,
                                           getSurgeBitmap(IDB_SWITCH_RETRIGGER));
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_oscroute:
         {
            CRect rect(0, 0, 22, 15);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new gui_hswitch(rect, this, p->id + start_paramtags, 3, 15, 1, 3,
                                            getSurgeBitmap(IDB_OSCROUTE), nopoint, true);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_envshape:
         {
            bool hasShape = synth->storage.getPatch()
                                .scene[current_scene]
                                .adsr[p->ctrlgroup_entry]
                                .mode.val.i == emt_digital;

            if (hasShape)
            {
               CRect rect(0, 0, 20, 14);
               rect.offset(p->posx, p->posy);
               gui_hswitch* hsw = new gui_hswitch(rect, this, p->id + start_paramtags, 3, 14, 1, 3,
                                                  getSurgeBitmap(IDB_ENVSHAPE), nopoint, true);
               hsw->setValue(p->get_value_f01());
               if (p->name[0] == 'd')
                  hsw->imgoffset = 3;
               else if (p->name[0] == 'r')
                  hsw->imgoffset = 6;

               frame->addView(hsw);
            }
         }
         break;
         case ct_envmode:
         {
            CRect rect(0, 0, 34, 15);
            rect.offset(p->posx, p->posy);
            gui_hswitch* hsw = new gui_hswitch(rect, this, p->id + start_paramtags, 2, 15, 2, 1,
                                               getSurgeBitmap(IDB_ENVMODE), nopoint, false);
            hsw->setValue(p->get_value_f01());

            frame->addView(hsw);
         }
         break;
         case ct_lfotrigmode:
         {
            CRect rect(0, 0, 51, 39);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new gui_hswitch(rect, this, p->id + start_paramtags, 3, 39, 3, 1,
                                            getSurgeBitmap(IDB_LFOTRIGGER), nopoint, true);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_bool_mute:
         {
            CRect rect(0, 0, 22, 15);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new gui_switch(rect, this, p->id + start_paramtags,
                                           getSurgeBitmap(IDB_SWITCH_MUTE));
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_bool_solo:
         {
            CRect rect(0, 0, 22, 15);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new gui_switch(rect, this, p->id + start_paramtags,
                                           getSurgeBitmap(IDB_SWITCH_SOLO));
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_bool_unipolar:
         {
            CRect rect(0, 0, 51, 15);
            rect.offset(p->posx, p->posy);
            CControl* hsw =
                new gui_switch(rect, this, p->id + start_paramtags, getSurgeBitmap(IDB_UNIPOLAR));
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_bool_relative_switch:
         {
            CRect rect(0, 0, 12, 18);
            rect.offset(p->posx + 129, p->posy + 5);
            CControl* hsw = new gui_switch(rect, this, p->id + start_paramtags,
                                           getSurgeBitmap(IDB_RELATIVE_TOGGLE));
            rect(1, 1, 9, 14);
            rect.offset(p->posx + 129, p->posy + 5);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_bool_link_switch:
         {
            CRect rect(0, 0, 12, 18);
            rect.offset(p->posx + 129, p->posy + 5);
            CControl* hsw = new gui_switch(rect, this, p->id + start_paramtags,
                                           getSurgeBitmap(IDB_SWITCH_LINK));
            rect(1, 1, 9, 14);
            rect.offset(p->posx + 129, p->posy + 5);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_osctype:
         {
            CRect rect(0, 0, 41, 18);
            rect.offset(p->posx + 96, p->posy + 1);
            CControl* hsw =
                new COscMenu(rect, this, tag_osc_menu, &synth->storage,
                             &synth->storage.getPatch().scene[current_scene].osc[current_osc]);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_fxtype:
         {
            CRect rect(6, 0, 131, 15);
            rect.offset(p->posx, p->posy);
            // CControl *m = new
            // CFxMenu(rect,this,tag_fx_menu,&synth->storage,&synth->storage.getPatch().fx[current_fx],current_fx);
            CControl* m = new CFxMenu(rect, this, tag_fx_menu, &synth->storage,
                                      &synth->storage.getPatch().fx[current_fx],
                                      &synth->fxsync[current_fx], current_fx);
            m->setValue(p->get_value_f01());
            frame->addView(m);
         }
         break;
         case ct_wstype:
         {
            CRect rect(0, 0, 28, 47);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new gui_hswitch(rect, this, p->id + start_paramtags, 6, 47, 6, 1,
                                            getSurgeBitmap(IDB_WAVESHAPER), nopoint, true);
            rect(0, 0, 28, 47);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_polymode:
         {
            CRect rect(0, 0, 50, 47);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new gui_hswitch(rect, this, p->id + start_paramtags, 6, 47, 6, 1,
                                            getSurgeBitmap(IDB_POLYMODE), nopoint, true);
            rect(0, 0, 50, 47);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_fxbypass:
         {
            CRect rect(0, 0, 135, 27);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new gui_hswitch(rect, this, p->id + start_paramtags, 4, 27, 1, 4,
                                            getSurgeBitmap(IDB_FXBYPASS), nopoint, true);
            fxbypass_tag = p->id + start_paramtags;
            rect(2, 2, 133, 25);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_pitch_octave:
         {
            CRect rect(0, 0, 96, 18);
            rect.offset(p->posx, p->posy + 1);
            CControl* hsw = new gui_hswitch(rect, this, p->id + start_paramtags, 7, 18, 1, 7,
                                            getSurgeBitmap(IDB_OCTAVES), nopoint, true);
            rect(1, 0, 91, 14);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_fbconfig:
         {
            CRect rect(0, 0, 134, 52);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new gui_hswitch(rect, this, p->id + start_paramtags, 8, 52, 1, 8,
                                            getSurgeBitmap(IDB_FBCONFIG), nopoint, true);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            filterblock_tag = p->id + start_paramtags;
         }
         break;
         case ct_fmconfig:
         {
            CRect rect(0, 0, 134, 52);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new gui_hswitch(rect, this, p->id + start_paramtags, 4, 52, 1, 4,
                                            getSurgeBitmap(IDB_FMCONFIG), nopoint, true);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_scenemode:
         {
            CRect rect(0, 0, 36, 27);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new gui_hswitch(rect, this, p->id + start_paramtags, 3, 27, 3, 1,
                                            getSurgeBitmap(IDB_SCENEMODE), nopoint, true);
            rect(1, 1, 35, 27);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_lfoshape:
         {
            CRect rect(0, 0, 359, 85);
            rect.offset(p->posx, p->posy - 2);
            int lfo_id = p->ctrlgroup_entry - ms_lfo1;
            if ((lfo_id >= 0) && (lfo_id < n_lfos))
            {
               CControl* slfo = new gui_superlfo(
                   rect, lfo_id == 0, this, p->id + start_paramtags,
                   &synth->storage.getPatch().scene[current_scene].lfo[lfo_id], &synth->storage,
                   &synth->storage.getPatch().stepsequences[current_scene][lfo_id]);
               lfodisplay = slfo;
               frame->addView(slfo);
            }
         }
         break;
         case ct_scenesel:
         {
            CRect rect(0, 0, 51, 27);
            rect.offset(p->posx, p->posy);
            CControl* sceneswitch = new gui_hswitch(rect, this, tag_scene_select, 2, 27, 1, 2,
                                                    getSurgeBitmap(IDB_SCENESWITCH), nopoint);
            sceneswitch->setValue(p->get_value_f01());
            rect(1, 1, 50, 26);
            rect.offset(p->posx, p->posy);
            sceneswitch->setMouseableArea(rect);
            frame->addView(sceneswitch);
         }
         break;
         case ct_character:
         {
            CRect rect(0, 0, 135, 12);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new gui_hswitch(rect, this, p->id + start_paramtags, 3, 12, 1, 3,
                                            getSurgeBitmap(IDB_CHARACTER), nopoint, true);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_midikey:
         {
            CRect rect(0, 0, 43, 14);
            rect.offset(p->posx, p->posy);
            CParamEdit* key = new CParamEdit(rect, this, p->id + start_paramtags);
            key->setControlMode(cm_notename);
            // key->altlook = true;
            key->setValue(p->get_value_f01());
            frame->addView(key);
         }
         break;
         case ct_pbdepth:
         {
            CRect rect(0, 0, 24, 10);
            rect.offset(p->posx, p->posy);
            CParamEdit* pbd = new CParamEdit(rect, this, p->id + start_paramtags);
            pbd->altlook = true;
            pbd->setControlMode(cm_pbdepth);
            pbd->setValue(p->get_value_f01());
            frame->addView(pbd);
         }
         break;
         case ct_polylimit:
         {
            CRect rect(0, 0, 43, 14);
            rect.offset(p->posx, p->posy);
            CParamEdit* key = new CParamEdit(rect, this, p->id + start_paramtags);
            key->setControlMode(cm_polyphony);
            // key->setLabel("POLY");
            // key->setLabelPlacement(lp_below);
            key->setValue(p->get_value_f01());
            frame->addView(key);
            polydisp = key;
         }
         break;
         default:
         {
            if (synth->isValidModulation(p->id, modsource))
            {
               gui_slider* hs = new gui_slider(CPoint(p->posx, p->posy + p->posy_offset * yofs),
                                               style, this, p->id + start_paramtags, true);
               hs->setModMode(mod_editor ? 1 : 0);
               hs->setModValue(synth->getModulation(p->id, modsource));
               hs->setModPresent(synth->isModDestUsed(p->id));
               hs->setModCurrent(synth->isActiveModulation(p->id, modsource));
               hs->setValue(p->get_value_f01());
               hs->setLabel(p->get_name());
               hs->setMoveRate(p->moverate);
               frame->addView(hs);
               param[i] = hs;
            }
            else
            {
               gui_slider* hs = new gui_slider(CPoint(p->posx, p->posy + p->posy_offset * yofs),
                                               style, this, p->id + start_paramtags);
               hs->setValue(p->get_value_f01());
               hs->setDefaultValue(p->get_default_value_f01());
               hs->setLabel(p->get_name());
               hs->setMoveRate(p->moverate);
               frame->addView(hs);
               param[i] = hs;

               /*						if(p->can_temposync() && (style &
               kHorizontal))
               {
               CRect rect(0,0,14,18);
               rect.offset(p->posx+134,p->posy+5 + p->posy_offset*yofs);
               CControl *hsw = new
               gui_switch(rect,this,p->id+start_paramtags+tag_temposyncoffset,bmp_temposync);
               rect(1,1,11,14);
               rect.offset(p->posx+134,p->posy+5 + p->posy_offset*yofs);
               hsw->setMouseableArea(rect);
               hsw->setValue(p->temposync?1.f:0.f);
               frame->addView(hsw);
               }		*/
            }
         }
         break;
         }
      }
      i++;
   }

   // resonance link mode
   if (synth->storage.getPatch().scene[current_scene].f2_link_resonance.val.b)
   {
      i = synth->storage.getPatch().scene[current_scene].filterunit[1].resonance.id;
      if (param[i] && dynamic_cast<gui_slider*>(param[i]) != nullptr)
         ((gui_slider*)param[i])->disabled = true;
   }

   // pan2 control
   if ((synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i !=
        fb_stereo) &&
       (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i != fb_wide))
   {
      i = synth->storage.getPatch().scene[current_scene].width.id;
      if (param[i] && dynamic_cast<gui_slider*>(param[i]) != nullptr)
         ((gui_slider*)param[i])->disabled = true;
   }

   CRect aboutbrect(892 - 37, 526, 892, 526 + 12);

   gui_hswitch* b_about = new gui_hswitch(aboutbrect, this, tag_about, 1, 27, 1, 1,
                                          getSurgeBitmap(IDB_BUTTON_ABOUT), nopoint, false);
   frame->addView(b_about);

   infowindow = new gui_pdisplay(CRect(0, 0, 0, 0));
   frame->addView(infowindow);

   CRect wsize(0, 0, window_size_x, window_size_y);
   aboutbox = new CAboutBox(aboutbrect, this, 0, 0, wsize, nopoint);
   frame->addView(aboutbox);

   CRect dialogSize(148, 8, 598, 8 + 182);
   saveDialog = new CViewContainer(dialogSize);
   saveDialog->setBackground(getSurgeBitmap(IDB_STOREPATCH));
   saveDialog->setVisible(false);
   frame->addView(saveDialog);

   gui_hswitch* cancelButton =
       new gui_hswitch(CRect(CPoint(305, 147), CPoint(62, 25)), this, tag_store_cancel, 1, 25, 1, 1,
                       nullptr, nopoint, false);
   saveDialog->addView(cancelButton);
   gui_hswitch* okButton = new gui_hswitch(CRect(CPoint(373, 147), CPoint(62, 25)), this,
                                           tag_store_ok, 1, 25, 1, 1, nullptr, nopoint, false);
   saveDialog->addView(okButton);

   patchName = new CTextEdit(CRect(CPoint(96, 31), CPoint(340, 21)), this, tag_store_name);
   patchCategory = new CTextEdit(CRect(CPoint(96, 58), CPoint(340, 21)), this, tag_store_category);
   patchCreator = new CTextEdit(CRect(CPoint(96, 85), CPoint(340, 21)), this, tag_store_creator);
   patchComment = new CTextEdit(CRect(CPoint(96, 112), CPoint(340, 21)), this, tag_store_comments);

   patchName->setBackColor(kWhiteCColor);
   patchCategory->setBackColor(kWhiteCColor);
   patchCreator->setBackColor(kWhiteCColor);
   patchComment->setBackColor(kWhiteCColor);

   patchName->setFontColor(kBlackCColor);
   patchCategory->setFontColor(kBlackCColor);
   patchCreator->setFontColor(kBlackCColor);
   patchComment->setFontColor(kBlackCColor);

   patchName->setFrameColor(kGreyCColor);
   patchCategory->setFrameColor(kGreyCColor);
   patchCreator->setFrameColor(kGreyCColor);
   patchComment->setFrameColor(kGreyCColor);

   saveDialog->addView(patchName);
   saveDialog->addView(patchCategory);
   saveDialog->addView(patchCreator);
   saveDialog->addView(patchComment);

   editor_open = true;
   queue_refresh = false;
   frame->setDirty();
   frame->invalid();
   // frame->setDropActive(true); // TODO VSTGUI4
}

void sub3_editor::close_editor()
{
   editor_open = false;
   lfodisplay = 0;
   frame->removeAll(true);
   setzero(param);
}

#if !TARGET_VST3
bool sub3_editor::open(void* parent)
#else
bool PLUGIN_API sub3_editor::open(void* parent, const PlatformType& platformType)
#endif
{
#ifndef TARGET_VST3
   // !!! always call this !!!
   super::open(parent);

   PlatformType platformType = kDefaultNative;
#endif

   CRect wsize(0, 0, window_size_x, window_size_y);
   frame = new CFrame(wsize, this);
   frame->setBackground(getSurgeBitmap(IDB_BG));
   frame->open(parent, platformType);
   /*#if TARGET_AUDIOUNIT
           synth = (sub3_synth*)_effect;
   #elif TARGET_VST3
      //synth = (sub3_synth*)_effect; ??
   #else
           vstlayer *plug = (vstlayer*)_effect;
           if(!plug->initialized) plug->init();
           synth = (sub3_synth*)plug->plugin_instance;
   #endif*/

   open_editor();

   return true;
}

void sub3_editor::close()
{}
void sub3_editor::setParameter(long index, float value)
{
   if (!frame)
      return;
   if (!editor_open)
      return;
   if (index > synth->storage.getPatch().param_ptr.size())
      return;
   parameter* p = synth->storage.getPatch().param_ptr[index];

   // if(param[index])
   {
      // param[index]->setValue(value);
      int j = 0;
      while (j < 7)
      {
         if ((synth->refresh_ctrl_queue[j] > -1) && (synth->refresh_ctrl_queue[j] != index))
            j++;
         else
            break;
      }
      synth->refresh_ctrl_queue[j] = index;
      synth->refresh_ctrl_queue_value[j] = value;
   }
}

void decode_controllerid(char* txt, int id)
{
   int type = (id & 0xffff0000) >> 16;
   int num = (id & 0xffff);
   switch (type)
   {
   case 1:
      sprintf(txt, "NRPN %i", num);
      break;
   case 2:
      sprintf(txt, "RPN %i", num);
      break;
   default:
      sprintf(txt, "CC %i", num);
      break;
   };
}

int32_t sub3_editor::controlModifierClicked(CControl* control, CButtonState button)
{
   if (!synth)
      return 0;
   if (!frame)
      return 0;
   if (!editor_open)
      return 0;
   /*if((button&kRButton)&&modsource)
   {
   modsource = 0;
   queue_refresh = true;
   return 1;
   }*/
   if (button & (kMButton | kButton4 | kButton5))
   {
      toggle_mod_editing();

      /*mod_editor = !mod_editor;
      blinktimer = 0.f;
      blinkstate = false;
      refresh_mod();*/
      return 1;
   }
   long tag = control->getTag();

   if (button & kDoubleClick)
      button |= kControl;

   if (button & kRButton)
   {
      if (tag == tag_osc_select)
      {
         CRect r = control->getViewSize();
         CRect menuRect;
         CPoint where;
         frame->getCurrentMouseLocation(where);
         int a = limit_range((int)((3 * (where.x - r.left)) / r.getWidth()), 0, 2);
         frame->localToFrame(where);
         menuRect.offset(where.x, where.y);

         COptionMenu* contextMenu = new COptionMenu(menuRect, 0, 0, 0, 0, kNoDrawStyle);
         int eid = 0;
         int id_copy = -1, id_copymod = -1, id_paste = -1;
         char txt[256];
         sprintf(txt, "Osc %i", a + 1);
         contextMenu->addEntry(txt, eid++);
         contextMenu->addEntry("-", eid++);
         id_copy = eid;
         contextMenu->addEntry("Copy", eid++);
         id_copymod = eid;
         contextMenu->addEntry("Copy (with modulation)", eid++);
         if (synth->storage.get_clipboard_type() == cp_osc)
         {
            id_paste = eid;
            contextMenu->addEntry("Paste", eid++);
         }

         frame->addView(contextMenu); // add to frame
         contextMenu->setDirty();
         contextMenu->onMouseDown(where, kLButton); // <-- modal menu loop is here
         // pParent->looseFocus(pContext);
         int command = contextMenu->getLastResult();
         frame->removeView(contextMenu, true); // remove from frame and forget

         if (command >= 0)
         {
            if (command == id_copy)
            {
               synth->storage.clipboard_copy(cp_osc, current_scene, a);
            }
            else if (command == id_copymod)
            {
               synth->storage.clipboard_copy(cp_oscmod, current_scene, a);
            }
            else if (command == id_paste)
            {
               synth->clear_osc_modulation(current_scene, a);
               synth->storage.clipboard_paste(cp_osc, current_scene, a);
               queue_refresh = true;
            }
         }
         return 1;
      }

      if (tag == tag_scene_select)
      {
         CRect r = control->getViewSize();
         CRect menuRect;
         CPoint where;
         frame->getCurrentMouseLocation(where);
         int a = limit_range((int)((2 * (where.x - r.left)) / r.getWidth()), 0, 2);
         frame->localToFrame(where);
         menuRect.offset(where.x, where.y);
         COptionMenu* contextMenu = new COptionMenu(menuRect, 0, 0, 0, 0, kNoDrawStyle);
         int eid = 0;
         int id_copy = -1, id_paste = -1;
         char txt[256];
         sprintf(txt, "Scene %c", 'A' + a);
         contextMenu->addEntry(txt, eid++);
         contextMenu->addEntry("-", eid++);
         id_copy = eid;
         contextMenu->addEntry("Copy", eid++);
         if (synth->storage.get_clipboard_type() == cp_scene)
         {
            id_paste = eid;
            contextMenu->addEntry("Paste", eid++);
         }

         frame->addView(contextMenu); // add to frame
         contextMenu->setDirty();
         contextMenu->onMouseDown(where, kLButton); // <-- modal menu loop is here
         // pParent->looseFocus(pContext);
         int command = contextMenu->getLastResult();
         frame->removeView(contextMenu, true); // remove from frame and forget

         if (command >= 0)
         {
            if (command == id_copy)
            {
               synth->storage.clipboard_copy(cp_scene, a, -1);
            }
            else if (command == id_paste)
            {
               synth->storage.clipboard_paste(cp_scene, a, -1);
               queue_refresh = true;
            }
         }
         return 1;
      }
   }

   if ((tag >= tag_mod_source0) && (tag < tag_mod_source_end))
   {
      int modsource = tag - tag_mod_source0;

      if (button & kRButton)
      {
         CRect menuRect;
         CPoint where;
         frame->getCurrentMouseLocation(where);
         frame->localToFrame(where);
         menuRect.offset(where.x, where.y);
         COptionMenu* contextMenu =
             new COptionMenu(menuRect, 0, 0, 0, 0, kNoDrawStyle | kMultipleCheckStyle);
         int eid = 0;
         int id_clearallmr = -1, id_learnctrl = -1, id_clearctrl = -1, id_bipolar = -1,
             id_copy = -1, id_paste = -1, id_rename = -1;
         contextMenu->addEntry((char*)modsource_abberations[modsource], eid++);

         int n_md = 0;
         int n_total_md = synth->storage.getPatch().param_ptr.size();

         const int max_md = 4096;
         assert(max_md >= n_total_md);

         int clear_md[max_md];
         for (int md = 0; md < n_total_md; md++)
            clear_md[md] = -1;

         bool first_destination = true;

         // ska borja pa 0, men borjade pa 1 innan.. kanske finns en anledning?
         for (int md = 0; md < n_total_md; md++)
         {
            if (((md < n_global_params) || ((synth->storage.getPatch().param_ptr[md]->scene - 1) ==
                                            synth->storage.getPatch().scene_active.val.i)) &&
                synth->isActiveModulation(md, modsource))
            {
               if (first_destination)
               {
                  contextMenu->addEntry("-", eid++);
                  first_destination = false;
               }

               char tmptxt[256];
               sprintf(tmptxt, "Clear %s -> %s [%.2f]", (char*)modsource_abberations[modsource],
                       synth->storage.getPatch().param_ptr[md]->get_full_name(),
                       synth->getModDepth(md, modsource));
               clear_md[md] = eid;
               contextMenu->addEntry(tmptxt, eid++);

               n_md++;
            }
         }

         bool cancellearn = false;

         if (n_md)
         {
            id_clearallmr = eid;
            contextMenu->addEntry("Clear all routings", eid++);
         }
         int ccid = 0;
         int sc = limit_range(synth->storage.getPatch().scene_active.val.i, 0, 1);
         if (within_range(ms_ctrl1, modsource, ms_ctrl1 + n_customcontrollers - 1))
         {
            ccid = modsource - ms_ctrl1;
            contextMenu->addEntry("-", eid++);
            char txt[256];

            if (synth->learn_custom > -1)
               cancellearn = true;
            id_learnctrl = eid;
            if (cancellearn)
               contextMenu->addEntry("Abort learn controller", eid++);
            else
               contextMenu->addEntry("Learn controller [MIDI]", eid++);

            if (synth->storage.controllers[ccid] >= 0)
            {
               id_clearctrl = eid;
               char txt4[256];
               decode_controllerid(txt4, synth->storage.controllers[ccid]);
               sprintf(txt, "Clear controller [currently %s]", txt4);
               contextMenu->addEntry(txt, eid++);
            }

            contextMenu->addEntry("-", eid++);
            id_bipolar = eid;
            contextMenu->addEntry("Bipolar", eid++);
            contextMenu->checkEntry(
                id_bipolar,
                synth->storage.getPatch().scene[0].modsources[ms_ctrl1 + ccid]->is_bipolar());
            id_rename = eid;
            contextMenu->addEntry("Rename", eid++);
         }

         int lfo_id = isLFO(modsource) ? modsource - ms_lfo1 : -1;

         if (lfo_id >= 0)
         {
            contextMenu->addEntry("-", eid++);
            id_copy = eid;
            contextMenu->addEntry("Copy", eid++);
            if (synth->storage.get_clipboard_type() == cp_lfo)
            {
               id_paste = eid;
               contextMenu->addEntry("Paste", eid++);
            }
         }

         frame->addView(contextMenu); // add to frame
         contextMenu->setDirty();
         contextMenu->onMouseDown(where, kLButton); // <-- modal menu loop is here
         // pParent->looseFocus(pContext);
         int command = contextMenu->getLastResult();
         frame->removeView(contextMenu, true); // remove from frame and forget

         if (command >= 0)
         {
            if (command == id_clearallmr)
            {
               for (int md = 1; md < n_total_md; md++)
                  synth->clearModulation(md, modsource);
               refresh_mod();
            }
            else if (command == id_learnctrl)
            {
               if (cancellearn)
                  synth->learn_custom = -1;
               else
                  synth->learn_custom = ccid;
            }
            else if (command == id_clearctrl)
            {
               synth->storage.controllers[ccid] = -1;
               synth->storage.save_midi_controllers();
            }
            else if (command == id_bipolar)
            {
               bool bp =
                   !synth->storage.getPatch().scene[0].modsources[ms_ctrl1 + ccid]->is_bipolar();
               synth->storage.getPatch().scene[0].modsources[ms_ctrl1 + ccid]->set_bipolar(bp);

               float f =
                   synth->storage.getPatch().scene[0].modsources[ms_ctrl1 + ccid]->get_output01();
               control->setValue(f);
               ((gui_modsrcbutton*)control)->setBipolar(bp);
               /*((gui_slider*)metaparam[ccid])->setValue(f);
               ((gui_slider*)metaparam[ccid])->SetQuantitizedDispValue(f);
               ((gui_slider*)metaparam[ccid])->setBipolar(synth->storage.getPatch().scene[0].modsources[ms_ctrl1+ccid]->is_bipolar());
             */
            }
            else if (command == id_copy)
            {
               if (lfo_id >= 0)
                  synth->storage.clipboard_copy(cp_lfo, sc, lfo_id);
            }
            else if (command == id_paste)
            {
               if (lfo_id >= 0)
                  synth->storage.clipboard_paste(cp_lfo, sc, lfo_id);
               queue_refresh = true;
            }
            else if (command == id_rename)
            {
               spawn_miniedit_text(synth->storage.getPatch().CustomControllerLabel[ccid], 16);
               ((gui_modsrcbutton*)control)
                   ->setlabel(synth->storage.getPatch().CustomControllerLabel[ccid]);
               //((gui_slider*)metaparam[ccid])->setLabel(synth->storage.getPatch().CustomControllerLabel[ccid]);
               synth->updateDisplay();
            }
            for (int md = 0; md < n_total_md; md++)
            {
               if (clear_md[md] == command)
               {
                  synth->clearModulation(md, modsource);
                  refresh_mod();
               }
            }
         }
         // remove from frame and forget
         return 1;
      }
      return 0;
   }

   if (tag < start_paramtags)
      return 0;
   if (!(button & (kControl | kRButton)))
      return 0;

   int ptag = tag - start_paramtags;
   if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
   {
      parameter* p = synth->storage.getPatch().param_ptr[ptag];

      if ((button & kRButton) && (p->valtype == vt_float))
      {
         CRect menuRect;
         CPoint where;
         frame->getCurrentMouseLocation(where);
         menuRect.offset(where.x, where.y);
         frame->localToFrame(where);
         COptionMenu* contextMenu =
             new COptionMenu(menuRect, 0, 0, 0, 0, kNoDrawStyle | kMultipleCheckStyle);
         int eid = 0;
         int id_temposync = -1, id_clearallmr = -1, id_extendrange = -1, id_learnctrl = -1,
             id_clearctrl = -1, id_absolute = -1;
         contextMenu->addEntry((char*)p->get_name(), eid++);
         contextMenu->addEntry("-", eid++);
         char txt[256], txt2[256];
         p->get_display(txt);
         sprintf(txt2, "Value: %s", txt);
         contextMenu->addEntry(txt2, eid++);
         bool cancellearn = false;

         // if(p->can_temposync() || p->can_extend_range())	contextMenu->addEntry("-",eid++);
         if (p->can_temposync())
         {
            id_temposync = eid;
            contextMenu->addEntry("Temposync", eid++);
            contextMenu->checkEntry(id_temposync, p->temposync);
         }
         if (p->can_extend_range())
         {
            id_extendrange = eid;
            contextMenu->addEntry("Extend range", eid++);
            contextMenu->checkEntry(id_extendrange, p->extend_range);
         }
         if (p->can_be_absolute())
         {
            id_absolute = eid;
            contextMenu->addEntry("Absolute", eid++);
            contextMenu->checkEntry(id_absolute, p->absolute);
         }

         {
            id_learnctrl = eid;
            if (synth->learn_param > -1)
               cancellearn = true;
            if (cancellearn)
               contextMenu->addEntry("Abort learn controller", eid++);
            else
               contextMenu->addEntry("Learn controller [midi]", eid++);
         }

         if (p->midictrl >= 0)
         {
            id_clearctrl = eid;
            char txt4[256];
            decode_controllerid(txt4, p->midictrl);
            sprintf(txt, "Clear controller [currently %s]", txt4);
            contextMenu->addEntry(txt, eid++);
         }

         int n_ms = 0;
         int clear_ms[n_modsources];
         bool is_modulated = false;
         for (int ms = 1; ms < n_modsources; ms++)
            if (synth->isActiveModulation(ptag, ms))
               n_ms++;

         if (n_ms)
         {
            contextMenu->addEntry("-", eid++);
            for (int ms = 1; ms < n_modsources; ms++)
            {
               if (synth->isActiveModulation(ptag, ms))
               {
                  char tmptxt[256];
                  sprintf(tmptxt, "Clear %s -> %s [%.2f]", (char*)modsource_abberations[ms],
                          p->get_name(), synth->getModDepth(ptag, ms));
                  clear_ms[ms] = eid;
                  contextMenu->addEntry(tmptxt, eid++);
               }
               else
                  clear_ms[ms] = -1;
            }
            if (n_ms > 1)
            {
               id_clearallmr = eid;
               contextMenu->addEntry("Clear all", eid++);
            }
         }

         frame->addView(contextMenu); // add to frame
         contextMenu->setDirty();
         contextMenu->onMouseDown(where, kLButton); // <-- modal menu loop is here

         // pParent->looseFocus(pContext);

         int command = contextMenu->getLastResult();
         if (command >= 0)
         {
            if (command == id_temposync)
            {
               p->temposync = !p->temposync;
            }
            else if (command == id_extendrange)
            {
               p->extend_range = !p->extend_range;
            }
            else if (command == id_absolute)
            {
               p->absolute = !p->absolute;
            }
            else if (command == id_clearallmr)
            {
               for (int ms = 1; ms < n_modsources; ms++)
               {
                  synth->clearModulation(ptag, ms);
               }
               refresh_mod();
            }
            else if (command == id_learnctrl)
            {
               if (cancellearn)
                  synth->learn_param = -1;
               else
                  synth->learn_param = p->id;
            }
            else if (command == id_clearctrl)
            {

               // p->midictrl = -1;
               // TODO add so parameter for both scenes are cleared!
               if (ptag < n_global_params)
               {
                  p->midictrl = -1;
                  synth->storage.save_midi_controllers();
               }
               else
               {
                  int a = ptag;
                  if (ptag >= (n_global_params + n_scene_params))
                     a -= ptag;

                  synth->storage.getPatch().param_ptr[a]->midictrl = -1;
                  synth->storage.getPatch().param_ptr[a + n_scene_params]->midictrl = -1;
                  synth->storage.save_midi_controllers();
               }
            }
            {
               for (int ms = 1; ms < n_modsources; ms++)
               {
                  if (clear_ms[ms] == command)
                  {
                     synth->clearModulation(ptag, ms);
                     refresh_mod();
                  }
               }
            }
         }
         frame->removeView(contextMenu, true); // remove from frame and forget
         return 1;
      }
      else if (button & kControl)
      {
         if (synth->isValidModulation(ptag, modsource) && mod_editor)
         {
            synth->clearModulation(ptag, modsource);
            ((gui_slider*)control)->setModValue(synth->getModulation(p->id, modsource));
            ((gui_slider*)control)->setModPresent(synth->isModDestUsed(p->id));
            ((gui_slider*)control)->setModCurrent(synth->isActiveModulation(p->id, modsource));
            // control->setGhostValue(p->get_value_f01());
            oscdisplay->setDirty();
            return 1;
         }
         else
         {
            p->set_value_f01(p->get_default_value_f01());
            control->setValue(p->get_value_f01());
            if (oscdisplay && (p->ctrlgroup == 2))
               oscdisplay->setDirty();
            if (lfodisplay && (p->ctrlgroup == 6))
               lfodisplay->setDirty();
            return 0;
         }
      }
      else
         return 0;
   }
   return 0;
}

void sub3_editor::valueChanged(CControl* control)
{
   if (!frame)
      return;
   if (!editor_open)
      return;
   long tag = control->getTag();

   if ((tag >= tag_mod_source0) && (tag < tag_mod_source_end))
   {
      if (((gui_modsrcbutton*)control)->event_is_drag)
      {
         int t = (tag - tag_mod_source0);
         ((modulation_controller*)synth->storage.getPatch().scene[0].modsources[t])
             ->set_target01(control->getValue(), false);

         synth->sendParameterAutomation(t + metaparam_offset - ms_ctrl1, control->getValue());

         return;
      }
      else
      {
         int state = ((gui_modsrcbutton*)control)->get_state();
         int newsource = tag - tag_mod_source0;
         long buttons = 0; // context->getMouseButtons(); // temp fix vstgui 3.5
         bool ciep = ((gui_modsrcbutton*)control)->click_is_editpart && (newsource >= ms_lfo1);

         if (!ciep)
         {
            switch (state & 3)
            {
            case 0:
               modsource = newsource;

               mod_editor = false;
               // mod_editor = true;
               queue_refresh = true;
               break;
            case 1:
               modsource = newsource;
               mod_editor = true;
               refresh_mod();
               // queue_refresh = true;
               break;
            case 2:
               modsource = newsource;
               // modsource = 0;
               mod_editor = false;
               refresh_mod();
               // queue_refresh = true;
               break;
            };
         }
         //((gui_modsrcbutton*)control)->

         if (isLFO(newsource) && !(buttons & kShift))
         {
            if (modsource_editor != newsource)
            {
               modsource_editor = newsource;
               queue_refresh = true;
            }
         }
      }

      return;
   }

   if ((tag == f1subtypetag) || (tag == f2subtypetag))
   {
      int idx = (tag == f2subtypetag) ? 1 : 0;
      int a = synth->storage.getPatch().scene[current_scene].filterunit[idx].subtype.val.i + 1;
      int nn =
          fut_subcount[synth->storage.getPatch().scene[current_scene].filterunit[idx].type.val.i];
      if (a >= nn)
         a = 0;
      synth->storage.getPatch().scene[current_scene].filterunit[idx].subtype.val.i = a;
      if (!nn)
         ((gui_switch*)control)->ivalue = 0;
      else
         ((gui_switch*)control)->ivalue = a + 1;
      control->invalid();
      synth->switch_toggled_queued = true;
      return;
   }

   switch (tag)
   {
   case tag_scene_select:
   {
      synth->release_if_latched[synth->storage.getPatch().scene_active.val.i] = true;
      synth->storage.getPatch().scene_active.val.i = (int)(control->getValue() * 1.f + 0.5f);
      // synth->storage.getPatch().param_ptr[scene_select_pid]->set_value_f01(control->getValue());
      queue_refresh = true;
      return;
   }
   break;
   case tag_patchname:
   {
      int id = ((gui_patchname*)control)->sel_id;
      // synth->load_patch(id);
      synth->patchid_queue = id;
      synth->process_threadunsafe_operations();
      return;
   }
   break;
   case tag_mp_category:
   {
      if (control->getValue() > 0.5f)
         synth->increment_patch(1, 0);
      else
         synth->increment_patch(-1, 0);
      return;
   }
   break;
   case tag_mp_patch:
   {
      if (control->getValue() > 0.5f)
         synth->increment_patch(0, 1);
      else
         synth->increment_patch(0, -1);
      return;
   }
   break;
   case tag_osc_select:
   {
      current_osc = (int)(control->getValue() * 2.f + 0.5f);
      queue_refresh = true;
      return;
   }
   break;
   case tag_fx_select:
   {
      auto fxc = ((gui_fxconf*)control);
      int d = fxc->get_disable();
      synth->fx_suspend_bitmask = synth->storage.getPatch().fx_disable.val.i ^ d;
      synth->storage.getPatch().fx_disable.val.i = d;
      fxc->set_disable(d);

      int nfx = fxc->get_current();
      if (current_fx != nfx)
      {
         current_fx = nfx;
         queue_refresh = true;
      }
      return;
   }
   case tag_osc_menu:
   {
      synth->switch_toggled_queued = true;
      queue_refresh = true;
      synth->process_threadunsafe_operations();
      return;
   }
   break;
   case tag_fx_menu:
   {
      synth->load_fx_needed = true;
      // queue_refresh = true;
      synth->fx_reload[current_fx & 7] = true;
      synth->process_threadunsafe_operations();
      return;
   }
   case tag_about:
   {
      if (aboutbox)
         ((CAboutBox*)aboutbox)->boxShow();
   }
   break;
   case tag_store:
   {
#ifdef ISDEMO
#if MAC
      CFOptionFlags responseFlags;
      CFUserNotificationDisplayAlert(0, kCFUserNotificationPlainAlertLevel, 0, 0, 0,
                                     CFSTR("Demo limitation"),
                                     CFSTR("Saving patches isn't supported in the demo version of "
                                           "Surge. (including sequencer recall)"),
                                     CFSTR("Ok"), 0, 0, &responseFlags);
#else
      MessageBox(::GetActiveWindow(),
                 L"Saving patches isn't supported in the demo version of Surge. (including "
                 L"sequencer recall)",
                 L"Demo limitation", MB_OK | MB_ICONWARNING);
#endif
#else
      patchdata p;
      p.name = synth->storage.getPatch().name;
      p.category = synth->storage.getPatch().category;
      p.comments = synth->storage.getPatch().comment;
      p.author = synth->storage.getPatch().author;
      if (p.author.empty())
         p.author = synth->storage.defaultname;
      if (p.comments.empty())
         p.comments = synth->storage.defaultsig;

      patchName->setText(p.name.c_str());
      patchCategory->setText(p.category.c_str());
      patchCreator->setText(p.author.c_str());
      patchComment->setText(p.comments.c_str());

      showPatchStoreDialog(&p, &synth->storage.patch_category,
                           synth->storage.patch_category_split[1]);
#endif
   }
   break;
   case tag_store_cancel:
   {
      saveDialog->setVisible(false);
      // frame->setModalView(nullptr);
      frame->setDirty();
   }
   break;
   case tag_store_ok:
   {
      saveDialog->setVisible(false);
      // frame->setModalView(nullptr);
      frame->setDirty();

      synth->storage.getPatch().name = patchName->getText();
      synth->storage.getPatch().author = patchName->getText();
      synth->storage.getPatch().category = patchCategory->getText();
      synth->storage.getPatch().comment = patchComment->getText();
      synth->save_patch();
   }
   break;
   default:
   {
      int ptag = tag - start_paramtags;
      if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
      {
         parameter* p = synth->storage.getPatch().param_ptr[ptag];

         char pname[256], pdisp[256], txt[256];
         bool modulate = false;

         // if(modsource && mod_active && (p->valtype == (valtypes)vt_float) && !((p->ctrlgroup ==
         // 6)&&(p->ctrlgroup_entry >= ms_lfo1)&&!can_modulate_modulators(modsource)) &&
         // (p->modulateable))
         if (modsource && mod_editor && synth->isValidModulation(p->id, modsource) &&
             dynamic_cast<gui_slider*>(control) != nullptr)
         {
            synth->setModulation(ptag, modsource, ((gui_slider*)control)->getModValue());
            ((gui_slider*)control)->setModPresent(synth->isModDestUsed(p->id));
            ((gui_slider*)control)->setModCurrent(synth->isActiveModulation(p->id, modsource));

            synth->getParameterName(ptag, txt);
            sprintf(pname, "%s -> %s", modsource_abberations_short[modsource], txt);
            sprintf(pdisp, "%f", synth->getModDepth(ptag, modsource));
            ((gui_pdisplay*)infowindow)->setLabel(pname, pdisp);
            modulate = true;

            if (is_customcontroller(modsource))
            {
               int ccid = modsource - ms_ctrl1;
               char* lbl = synth->storage.getPatch().CustomControllerLabel[ccid];

               if ((lbl[0] == '-') && !lbl[1])
               {
                  strncpy(lbl, p->get_name(), 15);
                  synth->storage.getPatch().CustomControllerLabel[ccid][15] = 0;
                  ((gui_modsrcbutton*)gui_modsrc[modsource])->setlabel(lbl);
                  synth->updateDisplay();
               }
            }
         }
         else
         {
            // synth->storage.getPatch().param_ptr[ptag]->set_value_f01(control->getValue());
            bool force_integer = frame->getCurrentMouseButtons() & kControl;
            if (synth->setParameter01(ptag, control->getValue(), false, force_integer))
            {
               queue_refresh = true;
               return;
            }
            else
            {
               synth->sendParameterAutomation(ptag, synth->getParameter01(ptag));

               if (dynamic_cast<gui_slider*>(control) != nullptr)
                  ((gui_slider*)control)->SetQuantitizedDispValue(p->get_value_f01());
               else
                  control->invalid();
               synth->getParameterName(ptag, pname);
               synth->getParameterDisplay(ptag, pdisp);
               ((gui_pdisplay*)infowindow)->setLabel(0, pdisp);
               if (p->ctrltype == ct_polymode)
                  modulate = true;
            }
         }
         if (!queue_refresh)
         {
            if (!(p->ctrlstyle & kNoPopup))
            {
               draw_infowindow(ptag, control, modulate);
            }

            if (oscdisplay && ((p->ctrlgroup == 2) || (p->ctrltype == ct_character)))
            {
               oscdisplay->setDirty();
               oscdisplay->invalid();
            }
            if (lfodisplay && (p->ctrlgroup == 6))
            {
               lfodisplay->setDirty();
               lfodisplay->invalid();
            }
         }
      }
      break;
   }
   }

   if ((tag == (f1subtypetag - 1)) || (tag == (f2subtypetag - 1)))
   {
      int idx = (tag == (f2subtypetag - 1)) ? 1 : 0;

      int a = synth->storage.getPatch().scene[current_scene].filterunit[idx].subtype.val.i;
      int nn =
          fut_subcount[synth->storage.getPatch().scene[current_scene].filterunit[idx].type.val.i];
      if (a >= nn)
         a = 0;
      synth->storage.getPatch().scene[current_scene].filterunit[idx].subtype.val.i = a;
      if (!nn)
         ((gui_switch*)filtersubtype[idx])->ivalue = 0;
      else
         ((gui_switch*)filtersubtype[idx])->ivalue = a + 1;

      filtersubtype[idx]->setDirty();
      filtersubtype[idx]->invalid();
   }

   if (tag == filterblock_tag)
   {
      // pan2 control
      int i = synth->storage.getPatch().scene[current_scene].width.id;
      if (param[i] && dynamic_cast<gui_slider*>(param[i]) != nullptr)
         ((gui_slider*)param[i])->disabled =
             (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i !=
              fb_stereo) &&
             (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i !=
              fb_wide);
      param[i]->setDirty();
   }
   if (tag ==
       fxbypass_tag) // still do the normal operation, that's why it's outside the switch-statement
   {
      if (ccfxconf)
         ((gui_fxconf*)ccfxconf)->set_bypass(synth->storage.getPatch().fx_bypass.val.i);

      switch (synth->storage.getPatch().fx_bypass.val.i)
      {
      case fxb_no_fx:
         synth->fx_suspend_bitmask = synth->fx_suspend_bitmask | 0xff;
         break;
      case fxb_scene_fx_only:
         synth->fx_suspend_bitmask = synth->fx_suspend_bitmask | 0xf0;
         break;
      case fxb_no_sends:
         synth->fx_suspend_bitmask = synth->fx_suspend_bitmask | 0x30;
         break;
      case fxb_all_fx:
      default:
         break;
      }
   }
}

//------------------------------------------------------------------------------------------------

void sub3_editor::beginEdit(long index)
{
#if !AU
   if (index < start_paramtags)
   {
      return;
   }

   int externalparam = synth->remapInternalToExternalApiId(index - start_paramtags);

   if (externalparam >= 0)
   {
      super::beginEdit(externalparam);
   }
#endif
}

//------------------------------------------------------------------------------------------------

void sub3_editor::endEdit(long index)
{
#if !AU
   if (index < start_paramtags)
   {
      return;
   }

   int externalparam = synth->remapInternalToExternalApiId(index - start_paramtags);

   if (externalparam >= 0)
   {
      super::endEdit(externalparam);
   }
#endif
}

//------------------------------------------------------------------------------------------------

void sub3_editor::controlBeginEdit(VSTGUI::CControl* control)
{
#if AU
   long tag = control->getTag();
   int ptag = tag - start_paramtags;
   if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
   {
      int externalparam = synth->remapInternalToExternalApiId(ptag);
      if (externalparam >= 0)
      {
         ((aulayer*)synth->parent)->ParameterBeginEdit(externalparam);
      }
   }
#endif
}

//------------------------------------------------------------------------------------------------

void sub3_editor::controlEndEdit(VSTGUI::CControl* control)
{
#if AU
   long tag = control->getTag();
   int ptag = tag - start_paramtags;
   if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
   {
      int externalparam = synth->remapInternalToExternalApiId(ptag);
      if (externalparam >= 0)
      {
         ((aulayer*)synth->parent)->ParameterEndEdit(externalparam);
      }
   }
#endif
   if (((gui_pdisplay*)infowindow)->isVisible())
   {
      ((gui_pdisplay*)infowindow)->Hide();
   }
}

//------------------------------------------------------------------------------------------------

void sub3_editor::draw_infowindow(int ptag, CControl* control, bool modulate, bool forceMB)
{
   long buttons = 1; // context?context->getMouseButtons():1;

   if (buttons && forceMB)
      return; // don't draw via CC if MB is down

   CRect r(0, 0, 148, 18);
   if (modulate)
      r.bottom += 18;
   CRect r2 = control->getViewSize();

   r.offset((r2.left / 150) * 150, r2.bottom);

   int ao = 0;
   if (r.bottom > window_size_y)
      ao = (window_size_y - r.bottom);
   r.offset(0, ao);

   if (buttons || forceMB)
   {
      ((gui_pdisplay*)infowindow)->setViewSize(r);
      ((gui_pdisplay*)infowindow)->Show();
      infowindow->invalid();
      clear_infoview_countdown = 40;
   }
   else
   {
      ((gui_pdisplay*)infowindow)->Hide();
      frame->invalidRect(r);
   }
}

bool sub3_editor::showPatchStoreDialog(patchdata* p,
                                       vector<patchlist_category>* patch_category,
                                       int startcategory)
{
   saveDialog->setVisible(true);
   // frame->setModalView(saveDialog);

   return false;
}

//------------------------------------------------------------------------------------------------
