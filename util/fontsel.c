/*******************************************************************************
*                                                                              *
* fontsel.c                                                                    *
*                                                                              *
* Copyright (C) 2018 Olaf Wintermann                                           *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fontconfig/fontconfig.h>

#include <Xm/XmAll.h>

#include "nedit_malloc.h"
#include "misc.h"

#include "fontsel.h"

typedef struct FontSelector {
    FcPattern *filter;
    FcFontSet *list;
    
    Widget pattern;
    Widget fontlist;
    Widget size;
    Widget name;
    
    int end;
    int cancel;
} FontSelector;

static void UpdateFontList(FontSelector *sel, char *pattern)
{
    XmStringTable items;
    FcChar8 *name;
    int nfonts, nfound;
    
    if(sel->filter) {
        FcPatternDestroy(sel->filter);
        sel->filter = NULL;
    }
    if(pattern) {
        sel->filter = FcNameParse((FcChar8*)pattern);
    }
    if(!sel->filter) {
        sel->filter = FcPatternCreate();
    }
    
    FcObjectSet *os = FcObjectSetCreate();
    FcObjectSetAdd(os, FC_FAMILY);
    FcObjectSetAdd(os, FC_FULLNAME);
    sel->list = FcFontList(NULL, sel->filter, os);
    FcObjectSetDestroy(os);
    nfonts = sel->list->nfont;
    nfound = 0;
    
    items = NEditCalloc(sel->list->nfont, sizeof(XmString));
    for(int i=0;i<nfonts;i++) {
        name = NULL;
        FcPatternGetString(sel->list->fonts[i], FC_FULLNAME, 0, &name);
        if(name) {
            items[nfound] = XmStringCreateSimple((char*)name);
            nfound++;
        }
    }
    
    XtVaSetValues(sel->fontlist, XmNitems, items, XmNitemCount, nfound, NULL);
}

static XmString MatchFont(const char *name)
{
    XmString ret = NULL;
    FcPattern* pat = FcNameParse((const FcChar8*)name);
    
    FcConfigSubstitute(NULL, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);
    FcResult result;
    FcPattern* font = FcFontMatch(NULL, pat, &result);
    if(font) {
        FcChar8* name = NULL;
        if (FcPatternGetString(font, FC_FULLNAME, 0, &name) == FcResultMatch) {
            ret = XmStringCreateSimple(name);
        }
        FcPatternDestroy(font);
    }
    FcPatternDestroy(pat);
    
    return ret;
}

static void CreateSizeList(Widget w)
{
    XmStringTable items = NEditCalloc(30, sizeof(XmString));
    char buf[8];
    for(int i=0;i<30;i++) {
        snprintf(buf, 8, "%d", i+5);
        items[i] = XmStringCreateSimple(buf);
    }
    
     XtVaSetValues(w, XmNitems, items, XmNitemCount, 30, NULL);
     for(int i=0;i<30;i++) {
         XmStringFree(items[i]);
     }
     NEditFree(items);
}

static char* GetFontString(FontSelector *sel)
{
    XmStringTable table = NULL;
    XmString item = NULL;
    int count = 0;
    int i;
    char *font = NULL;
    char *size = NULL;
    size_t fontLen, sizeLen, outLen;
    char *out;
    
    XtVaGetValues(
            sel->fontlist,
            XmNselectedItems,
            &table,
            XmNselectedItemCount,
            &count,
            NULL);
    if(count > 0) {
        XmStringGetLtoR(table[0], XmFONTLIST_DEFAULT_TAG, &font);
    }
    
    if(!font) {
        return NULL;
    }
    
    XtVaGetValues(
            sel->size,
            XmNselectedItems,
            &table,
            XmNselectedItemCount,
            &count,
            NULL);
    if(count > 0) {
        XmStringGetLtoR(table[0], XmFONTLIST_DEFAULT_TAG, &size);
    }
    
    fontLen = font ? strlen(font) : 0;
    sizeLen = size ? strlen(size) : 0;
    
    if(size) {
        outLen = fontLen + sizeLen + 16;
        out = XtMalloc(outLen);
        snprintf(out, outLen, "%s:%s", font, size);
        XmTextSetString(sel->name, out);
        XtFree(size);
        XtFree(font);
        return out;
    } else {
        return font;
    }
}

static void UpdateFontName(FontSelector *sel)
{
    char *fontStr = GetFontString(sel);
    if(fontStr) {
        XmTextSetString(sel->name, fontStr);
        XtFree(fontStr);
    }
}

static void fontlist_callback(Widget w, FontSelector *sel, XtPointer data)
{
    UpdateFontName(sel);
}

void size_callback (Widget w, FontSelector *sel, XtPointer data)
{
    UpdateFontName(sel);
}

static void ok_callback(Widget w, FontSelector *sel, XtPointer data)
{
    sel->end = 1;
}

static void cancel_callback(Widget w, FontSelector *sel, XtPointer data)
{
    sel->end = 1;
    sel->cancel = 1;
}

static void FreeFontSelector(FontSelector *sel)
{
    FcPatternDestroy(sel->filter);
    // TODO: free all stuff
    NEditFree(sel);
}

char *FontSel(Widget parent, const char *currFont)
{
    Arg args[32];
    int n = 0;
    XmString str;
    
    FontSelector *sel = NEditMalloc(sizeof(FontSelector));
    memset(sel, 0, sizeof(FontSelector));
    
    Widget dialog = CreateDialogShell(parent, "Font Selector", args, 0);
    AddMotifCloseCallback(dialog, (XtCallbackProc)cancel_callback, sel);
    Widget form = XmCreateForm(dialog, "form", args, 0);
    
    /* ok button */
    n = 0;
    str = XmStringCreateSimple("OK");
    XtSetArg(args[n], XmNtopOffset, 2); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNbottomOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget okButton = XmCreatePushButton(form, "button", args, n);
    XtManageChild(okButton);
    XmStringFree(str);
    XtAddCallback(
                okButton,
                XmNactivateCallback,
                (XtCallbackProc)ok_callback,
                sel);
    
    /* cancel button */
    n = 0;
    str = XmStringCreateSimple("Cancel");
    XtSetArg(args[n], XmNtopOffset, 2); n++;
    XtSetArg(args[n], XmNrightOffset, 5); n++;
    XtSetArg(args[n], XmNbottomOffset, 5); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget cancelButton = XmCreatePushButton(form, "button", args, n);
    XtManageChild(cancelButton);
    XmStringFree(str);
    XtAddCallback(
                cancelButton,
                XmNactivateCallback,
                (XtCallbackProc)cancel_callback,
                sel);
    
    /* font name */
    n = 0;
    XtSetArg(args[n], XmNbottomOffset, 2); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNrightOffset, 5); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNwidth, 400); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, okButton); n++;
    sel->name = XmCreateText(form, "fcname_textfield", args, n);
    XtManageChild(sel->name);
    
    /* font name label */
    n = 0;
    str = XmStringCreateSimple("Font name:");
    XtSetArg(args[n], XmNbottomOffset, 2); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, sel->name); n++;
    Widget fontNameLabel = XmCreateLabel(form, "label", args, n);
    XtManageChild(fontNameLabel);
    XmStringFree(str);
    
    /* label */
    n = 0;
    str = XmStringCreateSimple("Font:");
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, 5); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget fontListLabel = XmCreateLabel(form, "label", args, n);
    XtManageChild(fontListLabel);
    XmStringFree(str);
    
    n = 0;
    str = XmStringCreateSimple("Size:");
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, 5); n++;
    XtSetArg(args[n], XmNrightOffset, 5); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget fontSizeLabel = XmCreateLabel(form, "label", args, n);
    XtManageChild(fontSizeLabel);
    XmStringFree(str);
    
    /* font list */
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, fontListLabel); n++;
    XtSetArg(args[n], XmNtopOffset, 2); n++;
    XtSetArg(args[n], XmNbottomOffset, 2); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, fontNameLabel); n++;
    XtSetArg(args[n], XmNvisibleItemCount, 10); n++;
    sel->fontlist = XmCreateScrolledList(form, "fontlist", args, n);
    AddMouseWheelSupport(sel->fontlist);
    XtManageChild(sel->fontlist);
    XtAddCallback(
                sel->fontlist,
                XmNbrowseSelectionCallback,
                (XtCallbackProc)fontlist_callback,
                sel);
    
    /* font size list */
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, fontSizeLabel); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, sel->fontlist); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, fontNameLabel); n++;
    XtSetArg(args[n], XmNleftOffset, 2); n++;
    XtSetArg(args[n], XmNbottomOffset, 2); n++;
    XtSetArg(args[n], XmNtopOffset, 2); n++;
    sel->size = XmCreateScrolledList(form, "sizelist", args, n);
    CreateSizeList(sel->size);
    XtAddCallback (sel->size, XmNbrowseSelectionCallback, (XtCallbackProc)size_callback, sel);
    XmListSelectPos(sel->size, 6, 0);
    XtManageChild(sel->size);
    
    /*
    CreateSizeList(sel->size);
    XtManageChild(sel->size);
    str = XmStringCreateSimple("10");
    XmComboBoxSelectItem(sel->size, str);
    XtAddCallback (sel->size, XmNselectionCallback, (XtCallbackProc)size_callback, sel);
    XmStringFree(str);
    */
    
    UpdateFontList(sel, NULL);
    XmString selection = MatchFont(currFont);
    if(selection) {
        XmListSelectItem(sel->fontlist, selection, 0);
        XmStringFree(selection);
    }
    
    ManageDialogCenteredOnPointer(form);
    
    XtAppContext app = XtWidgetToApplicationContext(dialog);
    while(!sel->end && !XtAppGetExitFlag(app)) {
        XEvent event;
        XtAppNextEvent(app, &event);
        XtDispatchEvent(&event);
    }
    
    XtDestroyWidget(dialog);
    char *retStr = NULL;
    if(!sel->cancel) {
        retStr = GetFontString(sel);
    }
    if(!retStr) {
        retStr = NEditStrdup(currFont);
    }
    
    FreeFontSelector(sel);
    return retStr;
}

