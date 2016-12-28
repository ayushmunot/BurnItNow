/*
 * Copyright 2010-2012, BurnItNow Team. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "CompilationAudioView.h"
#include "CommandThread.h"

#include <Alert.h>
#include <ControlLook.h>
#include <Directory.h>
#include <Entry.h>
#include <LayoutBuilder.h>
#include <Node.h>
#include <NodeInfo.h>
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>

static const float kControlPadding = be_control_look->DefaultItemSpacing();

// Message constants
const int32 kBurnerMessage = 'Brnr';
const int32 kBurnDiscMessage = 'BURN';

CompilationAudioView::CompilationAudioView(BurnWindow& parent)
	:
	BView("Audio", B_WILL_DRAW, new BGroupLayout(B_VERTICAL, kControlPadding)),
	fBurnerThread(NULL)
{
	windowParent = &parent;
	fCurrentPath = 0;
	
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	fBurnerInfoBox = new BSeparatorView(B_HORIZONTAL, B_FANCY_BORDER);
	fBurnerInfoBox->SetFont(be_bold_font);
	fBurnerInfoBox->SetLabel("Ready");

	fBurnerInfoTextView = new BTextView("AudioInfoTextView");
	fBurnerInfoTextView->SetWordWrap(false);
	fBurnerInfoTextView->MakeEditable(false);
	BScrollView* infoScrollView = new BScrollView("AudioInfoScrollView",
		fBurnerInfoTextView, 0, true, true);

	fBurnButton = new BButton("BurnDiscButton", "Burn disc",
		new BMessage(kBurnDiscMessage));
	fBurnButton->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	fAudioBox = new BSeparatorView(B_HORIZONTAL, B_FANCY_BORDER);
	fAudioBox->SetFont(be_bold_font);
	fAudioBox->SetLabel("Drop tracks here (only WAV files)");

	fTrackList = new AudioListView("AudioListView");
	BScrollView* audioScrollView = new BScrollView("AudioScrollView",
		fTrackList, 0, true, true);

	BLayoutBuilder::Group<>(dynamic_cast<BGroupLayout*>(GetLayout()))
		.SetInsets(kControlPadding)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fBurnButton)
			.AddGlue()
			.End()
		.AddGroup(B_HORIZONTAL)
			.AddGroup(B_VERTICAL)
				.Add(fBurnerInfoBox)
				.Add(infoScrollView)
				.End()
			.AddGroup(B_VERTICAL)
				.Add(fAudioBox)
				.Add(audioScrollView)
				.End()
			.End();
}


CompilationAudioView::~CompilationAudioView()
{
	delete fBurnerThread;
}


#pragma mark -- BView Overrides --


void
CompilationAudioView::AttachedToWindow()
{
	BView::AttachedToWindow();

	fBurnButton->SetTarget(this);
	fBurnButton->SetEnabled(false);
}


void
CompilationAudioView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kBurnerMessage:
			_BurnerParserOutput(message);
			break;
		case kBurnDiscMessage:
			BurnDisc();
			break;
		case B_REFS_RECEIVED:
			_AddTrack(message);
			break;
		default:
			BView::MessageReceived(message);
	}
}


#pragma mark -- Private Methods --


void
CompilationAudioView::_BurnerParserOutput(BMessage* message)
{
	BString data;

	if (message->FindString("line", &data) == B_OK) {
		data << "\n";
		fBurnerInfoTextView->Insert(data.String());
	}
	int32 code = -1;
	if (message->FindInt32("thread_exit", &code) == B_OK) {
		if (code == 0) {
			fBurnerInfoBox->SetLabel("Burning complete. Burn another disc?");
			fBurnButton->SetEnabled(true);
		}
	}
}


void
CompilationAudioView::_AddTrack(BMessage* message)
{
	int32 index;
	if (message->WasDropped()) {
		BPoint dropPoint = message->DropPoint();
		index = fTrackList->IndexOf(fTrackList->ConvertFromScreen(dropPoint));
		if (index < 0)
			index = fTrackList->CountItems();
	}

	entry_ref trackRef;
	int32 i = 0;
	while (message->FindRef("refs", i, &trackRef) == B_OK) {
		BEntry entry(&trackRef, true);	// also accept symlinks
		BNode node(&entry);
		if (node.InitCheck() != B_OK)
			return;

		BNodeInfo nodeInfo(&node);
		if (nodeInfo.InitCheck() != B_OK)
			return;

		char mimeTypeString[B_MIME_TYPE_LENGTH];
		nodeInfo.GetType(mimeTypeString);
		BPath* trackPath = new BPath(&entry);
		BString filename(trackPath->Leaf());
		BString path(trackPath->Path());

		// Check for wav MIME type or file extension
		if ((strcmp("audio/x-wav", mimeTypeString) == 0)
			|| filename.IFindLast(".wav", filename.CountChars())
				== filename.CountChars() - 4) {
			if (!message->WasDropped())
				index = fTrackList->CountItems();
			fTrackList->AddItem(new AudioListItem(filename, path, i), index++);
		}
		if (node.IsDirectory()) {
			BDirectory dir(&entry);
			entry_ref ref;

			while (dir.GetNextRef(&ref) == B_OK) {
				BNode dNode(&ref);
				if (dNode.InitCheck() != B_OK)
					return;

				BNodeInfo dNodeInfo(&dNode);
				if (dNodeInfo.InitCheck() != B_OK)
					return;

				dNodeInfo.GetType(mimeTypeString);

				BPath* dTrackPath = new BPath(&ref);
				BString dFilename(dTrackPath->Leaf());
				BString dPath(dTrackPath->Path());

				// Check for wav MIME type or file extension
				if ((strcmp("audio/x-wav", mimeTypeString) == 0)
					|| dFilename.IFindLast(".wav", dFilename.CountChars())
						== dFilename.CountChars() - 4) {
					if (!message->WasDropped())
						index = fTrackList->CountItems();

					fTrackList->AddItem(new AudioListItem(dFilename, dPath, i),
						index++);
				}
			}
		}
		i++;
	}
	if (!fTrackList->IsEmpty()) {
		fBurnButton->SetEnabled(true);
		fTrackList->RenumberTracks();
	} else
		fBurnButton->SetEnabled(false);
}


#pragma mark -- Public Methods --


void
CompilationAudioView::BurnDisc()
{
	if (fTrackList->IsEmpty())
		return;

	fBurnerInfoTextView->SetText(NULL);
	fBurnerInfoBox->SetLabel("Burning in progress" B_UTF8_ELLIPSIS);
	fBurnButton->SetEnabled(false);

	BString device("dev=");
	device.Append(windowParent->GetSelectedDevice().number.String());
	sessionConfig config = windowParent->GetSessionConfig();
	
	fBurnerThread = new CommandThread(NULL,
		new BInvoker(new BMessage(kBurnerMessage), this));
	
	fBurnerThread->AddArgument("cdrecord");

	if (config.simulation)
		fBurnerThread->AddArgument("-dummy");
	if (config.eject)
		fBurnerThread->AddArgument("-eject");
	if (config.mode == "-sao")
		fBurnerThread->AddArgument("-dao");	// for max compatibility
	else
		fBurnerThread->AddArgument(config.mode);

	fBurnerThread->AddArgument("speed=4")	// for max compatibility
		->AddArgument("fs=4m")				// for max compatibility
		->AddArgument(device)
		->AddArgument("-audio")
		->AddArgument("-copy")
		->AddArgument("-pad")
		->AddArgument("padsize=63s");

	for (unsigned int i = 0; i <= MAX_TRACKS; i++) {
		AudioListItem* sItem = dynamic_cast<AudioListItem *>
			(fTrackList->ItemAt(i));

		if (sItem == NULL)
			break;

		BString track(sItem->GetPath());
		fBurnerThread->AddArgument(track);
	}
	fBurnerThread->Run();
}
