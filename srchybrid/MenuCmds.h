#pragma once

///////////////////////////////////////////////////////////////////////////////
// Menu commands for GUI only

// Menu entries for the application system menu -> require a set of IDs with special restrictions!
#define MP_RESTORE				0x110
#define MP_CONNECT				0x120
#define MP_DISCONNECT			0x130
#define MP_EXIT					0x140
#define	MP_ABOUTBOX				0x150
#define MP_VERSIONCHECK			0x160
#define MP_MINIMIZETOTRAY		0x170

#define MP_MESSAGE				10102
#define MP_DETAIL				10103
#define MP_ADDFRIEND			10104
#define MP_REMOVEFRIEND			10105
#define MP_SHOWLIST				10106
#define MP_FRIENDSLOT			10107

#define MP_CANCEL				10201
#define MP_STOP					10202
#define MP_PAUSE				10203
#define MP_RESUME				10204
#define	MP_CLEARCOMPLETED		10205
#define	MP_OPEN					10206
#define	MP_PREVIEW				10207
#define MP_CMT					10208
#define MP_HM_CON				10209
#define MP_HM_SRVR				10210
#define MP_HM_TRANSFER			10211
#define MP_HM_SEARCH			10212
#define MP_HM_FILES				10213
#define MP_HM_MSGS				10214
#define MP_HM_IRC				10215
#define MP_HM_STATS				10216
#define MP_HM_PREFS				10217
#define MP_HM_OPENINC			10218
#define MP_HM_EXIT				10219
#define MP_TRY_TO_GET_PREVIEW_PARTS 10220
#define MP_ADDSOURCE			10221
#define MP_ALL_A4AF_AUTO		10222
#define MP_META_DATA			10225
#define MP_BOOT					10226
#define MP_HM_CONVERTPF			10227
#define MP_RESUMEPAUSED			10228
#define MP_HM_KAD				10229
#define MP_HM_LINK1				10230
#define MP_HM_LINK2				10231
#define MP_HM_LINK3				10232
#define MP_HM_SCHEDONOFF		10233
#define MP_SELECTTOOLBARBITMAPDIR 10234
#define MP_SELECTTOOLBARBITMAP	10235
#define MP_NOTEXTLABELS			10236
#define MP_TEXTLABELS			10237
#define MP_TEXTLABELSONRIGHT	10238
#define	MP_CUSTOMIZETOOLBAR		10239
#define	MP_SELECT_SKIN_FILE		10240
#define	MP_SELECT_SKIN_DIR		10241
#define MP_HM_HELP				10242
#define MP_HM_1STSWIZARD		10243
#define MP_OPENFOLDER			10244
#define	MP_HM_IPFILTER			10245
#define	MP_WEBSVC_EDIT			10246
#define	MP_HM_DIRECT_DOWNLOAD	10247
#define	MP_INSTALL_SKIN			10248
#define	MP_LARGEICONS			10249
#define	MP_SMALLICONS			10250
#define	MP_VIEW2_CLIENTS		10251
#define	MP_VIEW2_DOWNLOADING	10252
#define	MP_VIEW2_UPLOADING		10253
#define	MP_VIEW2_ONQUEUE		10254
#define MP_VIEW1_SPLIT_WINDOW	10255
#define MP_VIEW1_UPLOADING		10256
#define MP_VIEW1_DOWNLOADS		10257
#define MP_VIEW1_ONQUEUE		10258
#define MP_VIEW1_DOWNLOADING	10259
#define MP_VIEW1_CLIENTS		10260
#define MP_PAUSEONPREVIEW		10261
#define MP_VIEW_KADCONTACTS		10262
#define MP_VIEW_KADLOOKUP		10263

#define MP_PRIOVERYLOW			10300
#define MP_PRIOLOW				10301
#define MP_PRIONORMAL			10302
#define MP_PRIOHIGH				10303
#define MP_PRIOVERYHIGH			10304
#define MP_GETED2KLINK			10305
#define MP_GETHTMLED2KLINK		10306
#define MP_METINFO				10307
#define MP_PERMALL				10308
#define MP_PERMFRIENDS			10309
#define MP_PERMNONE				10310
#define MP_CONNECTTO			10311
#define MP_REMOVE				10312
#define MP_REMOVEALL			10313
#define MP_REMOVESELECTED		10314
#define MP_UNBAN				10315
#define MP_ADDTOSTATIC			10316
#define MP_CLCOMMAND			10317
#define MP_PRIOAUTO				10317
#define MP_REMOVEFROMSTATIC		10318
#define MP_VIEWFILECOMMENTS		10319
#define MP_CAT_ADD				10321
#define MP_CAT_EDIT				10322
#define MP_CAT_REMOVE			10323
#define MP_SAVELOG				10324
#define MPG_DELETE				10325
#define	MP_COPYSELECTED			10326
#define	MP_SELECTALL			10327
#define	MP_AUTOSCROLL			10328
#define MP_RESUMENEXT			10329
#define MPG_ALTENTER			10330
#define MPG_F2					10331
#define	MP_RENAME				10332
#define	MP_FIND					10333
#define	MP_UNDO					10334
#define	MP_CUT					10335
#define	MP_PASTE				10336
#define MP_DOWNLOAD_ALPHABETICAL 10337
#define MP_A4AF_CHECK_THIS_NOW	10338
#define	MPG_CUT					10339
#define MP_GETKADSOURCELINK		10340
#define MP_SHOWED2KLINK			10341
#define MP_SETSOURCELIMIT		10342
#define MP_CREATECOLLECTION		10343
#define MP_VIEWCOLLECTION		10344
#define MP_MODIFYCOLLECTION		10345
#define MP_SHAREDIR				10346
#define MP_SHAREDIRSUB			10347
#define MP_UNSHAREDIR			10348
#define MP_UNSHAREDIRSUB		10349
#define MP_SEARCHRELATED		10350
#define MP_SEARCHAUTHOR			10351
#define MP_RESTORESEARCHPARAMS	10352
#define	MP_FILTER_RESET			10353
#define	MP_FILTER_APPLY_ALL		10354
#define	MP_FILTER_APPLY_SINGLE	10355
#define	MP_SHOW_FILESIZE_DFLT	10356
#define	MP_SHOW_FILESIZE_KBYTE	10357
#define	MP_SHOW_FILESIZE_MBYTE	10358
#define MP_MARKASSPAM			10359
#define MP_UNSHAREFILE			10360
#define MP_NEWCAT				10361
#define MP_TOGGLEDTOOLBAR		10362
#define MP_AUTOKADLOOKUPGRAPH	10363
#define MP_UPDATE				10364


// quick-speed changer
#define MP_QS_U10				10501
#define MP_QS_U20				10502
#define MP_QS_U30				10503
#define MP_QS_U40				10504
#define MP_QS_U50				10505
#define MP_QS_U60				10506
#define MP_QS_U70				10507
#define MP_QS_U80				10508
#define MP_QS_U90				10509
#define MP_QS_U100				10510
#define MP_QS_UPC				10511
#define MP_QS_UP10				10512
#define MP_QS_UPL				10513
#define MP_QS_D10				10521
#define MP_QS_D20				10522
#define MP_QS_D30				10523
#define MP_QS_D40				10524
#define MP_QS_D50				10525
#define MP_QS_D60				10526
#define MP_QS_D70				10527
#define MP_QS_D80				10528
#define MP_QS_D90				10529
#define MP_QS_D100				10530
#define MP_QS_DC				10531
#define MP_QS_DL				10532
#define MP_QS_PA				10533
#define MP_QS_UA				10534

#define MP_WEBURL				10600	// reserve 100 entries for weburls!
#define MP_ASSIGNCAT			10700	// reserve 100 entries for categories!
#define MP_SCHACTIONS			10800	// reserve 100 entries for schedules
#define MP_CAT_SET0				10900	// reserve 100 entries for change all-cats
#define MP_TOOLBARBITMAP		11000	// reserve 100 entries for toolbar bitmaps
#define	MP_SKIN_PROFILE			11100	// reserve 100 entries for skin profiles
#define	MP_PREVIEW_APP_MIN		11200	// reserve 50 entries for preview apps
#define	MP_PREVIEW_APP_MAX		(MP_PREVIEW_APP_MIN + 49)
#define MP_FILTERCOLUMNS		11300	// reserve 50 entries for filter columns

#define Irc_Join				10240
#define Irc_Close				10241
#define Irc_Priv				10242
#define Irc_AddFriend			10243
#define	Irc_SendLink			10244
#define Irc_SetSendLink			10245
#define Irc_Kick				10246
#define Irc_Ban					10247
#define Irc_KB					10248
#define Irc_Slap				10249
//Note: reserve at least 50 ID's (Irc_OpCommands - Irc_OpCommands + 49).
#define Irc_OpCommands			10250
//Note: reserve at least 100 ID's (Irc_ChanCommands - Irc_ChanCommands + 99).
#define Irc_ChanCommands		(Irc_OpCommands + 50)
