# Wispr Flow desktop app reference

Date: 2026-09-08

Feature and UI reference for Wispr Flow for Windows 1.6.793, read out of the shipped app bundles. Background for Speecher's own dictation UI work; nothing here is a Speecher requirement.


## Scope

This document describes the user-facing behavior present in the extracted Windows Electron build, version 1.6.793. Wispr Flow uses feature flags, account plans, organization policy, operating-system checks, and staged rollouts, so one user may not see every page or control listed here. A fact marked `(uncertain)` is suggested by the bundle but cannot be resolved to one visible behavior from the static resources alone.

## Visual character and window model

The app uses a warm, off-white interface with near-black text, pale sand borders, rounded cards, compact pills, and pink, lavender, green, orange, and blue accents. Figtree is the main interface typeface. Manrope, EB Garamond, Google Sans Code, and system or monospace faces are included for selected content. A dark palette is implemented in the shared styles, although the normal stored default is light. The Hub is a conventional sidebar-and-content window. The Flow Bar, context menu, Scratchpad, and Notepad are separate compact Electron windows that can stay near the work being dictated into.

## Feature list

### Dictation

Flow turns microphone speech into text in other desktop apps. The two main input methods are push-to-talk and hands-free mode. A user holds the configured dictation shortcut and speaks, or starts a continuing hands-free session. The app can play start and stop sounds, mute other audio while listening, paste the result into the active text field, and fall back to the clipboard if direct paste fails. The Hub keeps transcript and audio history when the applicable storage settings allow it.

### Smart Formatting and Auto Cleanup

Smart Formatting adds punctuation and structure and adapts output to the active app. The bundle includes specific handling and education for lists, email formatting, backtracking corrections, and IDE file tagging. Auto Cleanup controls how aggressively Flow rewrites every dictation. Its choices are None, Light, Medium, and High; Light is the stored default. The original transcript remains available through `Undo AI edit` in recent dictation actions.

### Polish and Transforms

Polish rewrites selected text and can paste the result automatically. The stored defaults enable Polish and automatic paste. The Transforms page also exposes `Prompt Engineer` and user-created transforms. A transform can have a shortcut, instructions, and writing samples. Users can inspect a before-and-after diff, accept edits, undo them, retry, rate the result, or open configuration. The Flow Bar reports states such as `Cleaned up!`, `See what’s changed`, failure, no selection, and no changes.

### Command Mode

Command Mode treats speech as an instruction rather than text to insert. It can act on selected text and supports spoken actions such as a final `press enter` when the optional Press Enter command is enabled. The feature has its own shortcut and first-use notices. Its stored default is off. Some accounts receive Instruct Mode instead, in which case the separate Command Mode experiment is hidden.

### Instruct Mode

Instruct Mode combines a spoken request with selected text or context read from the active screen. It can draft or revise text, show where its context came from, and open an instruct chat. An optional Gmail flow learns email-writing preferences from recent sent mail and uses that profile when drafting email. Availability and the exact relationship between Instruct Mode and Command Mode are feature-flagged.

### Whisper use

The onboarding microphone quiz teaches that Flow can understand quiet speech and includes whisper-oriented practice. The extracted state model does not show a persistent `Whisper mode` toggle or a separate whisper dictation state. In this build, whispering appears to be a way of using normal dictation rather than a mode the user switches on.

### Notetaker

Notetaker records microphone and system meeting audio, shows a live transcript, identifies speakers, and produces meeting notes, a summary, tasks, and a pre-meeting brief. It can open a side-by-side Notepad, join calendar meetings, start from a triple tap or hotkey, stop when a call ends, and enforce a maximum recording length. Notes can be private, team-visible, or link-visible. Ask Wispr offers meeting-aware chat with prompts such as `Coach me`, `List recent todos`, and a weekly recap. Imports from Granola and Otter are represented in the app.

### Scratchpad

Scratchpad is a detachable rich-text note window with multiple tabs, recent-note search, pinning, versions, transforms, and dictation. Its formatting menu includes bold, italic, underline, blockquote, bullet and numbered lists, checklists, inline code, links, tables, and images. A note can be copied or sent to the current app. `Add to Flow Bar` makes Scratchpad directly reachable from the floating bar. Notes can sync across desktop and mobile when cloud storage is available.

### Dictionary

Dictionary stores names, terms, and replacements that Flow should recognize. Users can add, edit, star, search, sort, import, and delete entries. A replacement rule maps a commonly misheard or misspelled form to the wanted text. Sorting offers `Newest first`, `Oldest first`, `Alphabetical (A-Z)`, and `Starred first`. Flow can automatically learn corrected words, and team accounts can share entries with a team or department.

### Snippets

Snippets expand a spoken trigger phrase into saved text. Users can create, edit, search, sort, import, share, and bulk-delete snippets. Expansion content can include rich text and links. Team and department scopes parallel the Dictionary sharing controls.

### Writing style

Style changes capitalization and punctuation by app category. The visible tabs are `Personal messages`, `Work messages`, `Email`, `Other`, and `Auto cleanup`. Each writing category offers `very casual`, `Casual`, `Formal.`, and `Excited!`, with a sample showing the effect. The bundle warns that style formatting applies only to English.

### Insights and voice profile

Insights shows total dictated words, average speed in words per minute, words corrected, dictionary fixes, daily and weekly streaks, a usage heatmap, and apps used. Tabs include `Your usage`, `Your voice`, and, for eligible teams, `Leaderboard`. Share cards include `Flow Highlights`, `Flow Streak`, `How fast you Flow`, `Where you Flow`, and `Your Voice Profile`. The team leaderboard can rank desktop words, mobile words, total words, speed, and streaks.

### Tasks

`My Tasks` collects action items from meeting notes and also accepts manually created tasks. Users can search, complete, archive, restore, or permanently delete them. Views are `By date created`, `By priority`, and `Archived`; priority sorting says it is coming soon. With Google Automations connected, a task can create a calendar event or send an email.

### Teams and shared content

Teams provide shared dictionaries and snippets, a team leaderboard, invitations, join requests, role and seat management, centralized billing, and organization controls. Roles shown in the member UI include Member, Admin, IT admin, and Super admin. Enterprise controls referenced by the plan and settings UI include SAML SSO, SCIM, domain capture, audit logs, managed deployment, app blocking, model-training policy, and HIPAA support.

### Connectors and MCP

Connectors supply calendar reminders and context for meeting briefs, summaries, and automations. The catalog represented in this build includes Google Calendar, Outlook, Slack, Gmail or Google Automations, Notion, GitHub, and Linear, although the last group depends on the newer connector system. MCP offers one-click cards for Claude and ChatGPT plus the manual server URL `https://api.wisprflow.ai/connect/mcp`. MCP can access Notetaker notes and transcripts, but the UI explicitly says it cannot access dictations.

### Focus Mode and coding support

Focus Mode blocks configured running apps and website domains while active and advertises the shortcut `opt+f`; it is an internal-email experiment in this build. Vibe Coding adds `Variable recognition (VS Code, Cursor, Windsurf)` through those editors’ screen-reader mode and `File Tagging in Chat (Cursor & Windsurf)`, which tags relevant files while dictating in chat.

### Onboarding and feature education

Setup covers sign-in, the microphone, dictation shortcuts, languages, practice dictation, personalization, and optional team steps. Notetaker onboarding can ask how meetings are currently recorded, connect a calendar and other apps, and teach reminders, briefs, live transcription, and speaker names. A separate feature-tour window and many coachmarks introduce Dictionary, Snippets, Style, Polish, Scratchpad, file tagging, mouse controls, Notetaker, and new releases. `Restart tutorial` runs setup again without removing plan or cloud data.

### Tray, menu bar, reminders, and updates

Flow remains available from its tray or menu-bar icon when its main window is closed. It can launch at login, show or hide its dock/taskbar presence, keep the Flow Bar visible, display the next meeting and countdown, and present meeting actions from the icon. Typing reminders can show the Flow Bar in selected app categories and can be hidden for one hour or muted for 12 hours. The settings sidebar reports update states such as `Checking for updates`, download progress, `Up to date`, and a ready-to-restart action. Windows update handling uses the Electron/Squirrel `Update.exe` mechanism.

## Main window

### Sidebar and account area

The primary navigation is headed by pages selected from `Home`, `My Voice`, `Dictionary`, `Snippets`, `Insights`, `Notetaker`, `Style`, `Transforms`, `Scratchpad`, `My Tasks`, and `Extensions`. Availability changes with feature flags and account state. When `My Voice` is enabled, it can replace the separate Dictionary and Snippets destinations. The secondary navigation contains `Invite your team`, `Refer a friend` or `Earn a prize`, `Shortcuts`, `Settings`, and `Help`. The account area shows plan or trial status, remaining Basic words, referral and mobile-download actions, and links to manage the account or team.

### Home

Home is the dictation-history page. It lists recent dictations with transcript text, source application, time, and, where retained, audio playback. Controls include search, play or pause, copy, feedback, retry, delete or archive, `Undo AI edit`, and a way to view changes after cleanup. The empty state reads `No dictations yet` and `Once you dictate across different apps, your entire dictation history will appear here.` Summary cards can show total words, WPM, day streak, and week streak.

### My Voice

My Voice is a feature-flagged combined voice-personalization destination. It brings voice-profile education and vocabulary-oriented controls into one page and may replace standalone Dictionary and Snippets navigation. Exact contents vary by rollout `(uncertain)`; the bundle still routes the underlying dictionary and snippet editors separately.

### Dictionary

The page has `Add new`, import, search, sorting, sync state, selection, and bulk actions. Entry editing includes the word, optional replacement behavior, star state, and sharing scope. Team users can switch between personal, team, and department vocabulary where allowed. Duplicate and import previews explain whether entries will be personal or shared.

### Snippets

The page mirrors Dictionary’s add, import, search, sort, sync, selection, and sharing structure. The editor pairs a spoken trigger with expansion content and supports links and rich text. Examples in the product education include personal links, introductions, and support replies. Bulk import previews identify duplicates and the target team or department.

### Insights

The header is `Insights`. `Your usage` contains productivity totals, WPM, streaks, the activity heatmap, corrections, and desktop app usage. `Your voice` presents voice-profile analysis and shareable profile material. `Leaderboard` shows team rank, historical weeks, desktop and mobile words, total words, WPM, and current daily streak; it can instead show create-team, invite, join-request, or enterprise-upgrade gates.

### Notetaker

The landing view shows `Upcoming meetings`, an `Up Next` area, and past notes with search and ownership filters such as My and Shared notes. Calendar cards can join a call and start Notetaker, start a note without joining, snooze reminders, or open notification settings. A meeting opens tabs for `Summary`, `Transcript`, `Tasks`, `Brief`, `Docs`, and `My thoughts`. Other controls cover editing the title, recording status, sharing, copying as Markdown, deleting, assigning or merging speaker names, imports, Ask Wispr, and retrying failed processing. Shared viewers may be denied the transcript until invited by email.

### Style

The page title is `Style`. Four app-category tabs choose among the style cards `very casual`, `Casual`, `Formal.`, and `Excited!`. The fifth tab, `Auto cleanup`, chooses None, Light, Medium, or High and shows examples of each level. A banner explains that Auto Cleanup applies to all dictations and points to `Undo AI edit` in Home.

### Transforms

The page title is `Transforms`. It lists built-in `Polish` and `Prompt Engineer` transforms and any custom transforms. Users can enable or disable Transforms, create or edit one, supply transformation instructions and writing samples, bind a shortcut, configure auto-paste, view changes, and reset built-ins to their defaults. Some controls and custom transforms depend on plan or rollout.

### Scratchpad

The Hub page provides searchable recent notes, `New note`, pin and delete actions, shortcut setup, and `Add to Flow Bar`. It also surfaces cloud-sync guidance. Opening a note uses the separate Scratchpad editor described under Other windows.

### My Tasks

The page title is `My Tasks`. Its toolbar has `Add task` and `Search tasks`; tabs are `By date created`, `By priority`, and `Archived`. Each row can change completion state and open its source meeting. Row and bulk menus offer Archive, Unarchive, Restore, and Delete. Automation actions use dialogs titled `Add to calendar` and `Send email`.

### Extensions

Extensions and per-extension detail routes are present behind a feature flag. The dependency UI can `Enable all` or `Disable all`; connector-backed tools for services such as GitHub and Linear are represented elsewhere in the bundle. The complete customer-facing extension catalog and whether this page replaces part of Connectors cannot be fixed from static flags `(uncertain)`.

### Help and feedback

`Help` opens feedback and support UI. Tabs include `Report issue` and `Share feedback`, with request type, message, contact fields, and image attachments. The older compact dialog uses `Send a message to the Flow team`, `Upload image`, and `Send message`. Failed submissions may copy the written feedback to the clipboard.

## Settings

Settings is divided into an account section and an app-settings section. Pages are conditional. The possible page labels are `General`, `System`, `Notetaker`, `Vibe Coding`, `Experimental`, `Connectors`, `MCP`, `Extensions`, `Internal`, `Testing`, `Account`, `Team` or `Organization`, `Plans & Billing`, and `Data & Privacy`.

### General

| Setting | Control and choices | Stored default or behavior |
| --- | --- | --- |
| `Shortcuts` | Button opening the shortcut editor | Opens `Choose your preferred shortcuts for using Flow.` |
| `Microphone` | Device picker | Current input device; Notetaker uses auto-detect according to the dialog subtitle |
| `Dictation Languages` or `Languages` | Multi-select searchable language picker | Empty selection is displayed as `Auto-detect (99 languages)` |
| `App Language` | Searchable dropdown with `System Default` | Feature-flagged; default is the system locale `(uncertain)` |
| `Email language` | Language dropdown | Conditional rollout; default is not explicit |

The shortcut editor supports up to several bindings per action and keyboard or mouse-button input. Actions are `Push to talk`, `Hands-free mode`, `Enter rebind`, `Instruct Mode` or `Command Mode`, `Paste last transcript`, `Copy last transcript`, `Open Scratchpad`, `Start Notetaker`, `Transform`, `View Transform changes`, and conditional `Focus Mode`. `Cancel` is fixed to Escape. Users can add bindings, remove them, and reset defaults. Exact key defaults are platform- and migration-dependent and are not recoverable as one Windows value from the bundled state `(uncertain)`.

### System

| Section | Setting | Type, choices, and default |
| --- | --- | --- |
| App settings | `Launch app at login` | Toggle, on |
| App settings | `Show Flow Bar at all times` | Toggle; backed by a hidden-versus-shown preference, exact first-run value uncertain |
| App settings | `Dictation reminder` | `Customize` button; Disabled, any previously used app, or selected categories: `AI apps`, `Document and notes`, `Email`, `Personal messengers`, `Work messengers` |
| App settings | `Show app in dock` | Toggle, on because `hideAppInDock` defaults off |
| Sound | `Dictation and notification sounds` | Toggle, on |
| Sound | `Mute music while dictating` | Toggle, on by default on Windows |
| Notifications | `Suggestions` | Toggle for setup and usage tips; default not explicit |
| Notifications | `Announcements` | Toggle for new features; default not explicit |
| Notifications | `Milestones` | Toggle for word counts, streaks, and referrals; default not explicit |
| Notifications | `Team updates` | Enterprise/team toggle; default not explicit |
| Notifications | `Team leaderboard updates` | Enterprise/team toggle; default not explicit |
| Scratchpad | `Scratchpad open behavior` | Dropdown: `Resume last note` (default), `Open in new tab`, `Open last active pinned note` |
| Extras | `Auto-add to dictionary` | Toggle, on |
| Extras | `Email auto signature` | Toggle, off; text choice `Spoken with Wispr Flow` or `Written with Wispr Flow`, with Written stored as default |
| Extras | `Creator mode` | Toggle, off; shows `Dictating with Wispr Flow` while dictating |
| Extras | `Smart Formatting` | Link to the Style page’s `Auto Cleanup` tab; formatting itself defaults on |
| Extras | `Add Wispr Flow to LinkedIn` | Conditional connector/action with View or Disconnect states |
| Data | `Restart tutorial` | Confirmation button; reruns setup and preserves history, settings, and plan |
| Data | `Reset app` | Destructive confirmation `Reset & restart`; deletes local data, then re-syncs saved dictionary, stats, and settings |

Some Notetaker controls are rendered in System in one layout and on the separate Notetaker page in another rollout. They are listed once in the next section.

### Notetaker

| Section | Setting | Type, choices, and default |
| --- | --- | --- |
| Meeting detection | `Notify before scheduled meetings start` | Dropdown: `Right before the meeting` or 15 sec, `1 minute before`, `2 minutes before`, `Never`; notifications are enabled by default, but the migrated timing value is uncertain |
| Meeting detection | `Show your next meeting in the menu bar` | Toggle, on; platform-conditional wording |
| Meeting detection | `Automatically detect any call` or `Auto Detect` | Toggle, on; detection can `Notify me` or `Do nothing` |
| Meeting detection | `Stop Notetaker when a call ends` | Toggle, on |
| Meeting detection | `Maximum recording length` | Dropdown: 30 minutes, 1 hour, 2 hours (default), 3 hours |
| Meeting indicator | `Don't show Notepad and Flow Bar in screen capture` or `Make Flow Bar and Notepad invisible while screen sharing` | Toggle, off |
| Notepad | `Open Notepad when starting Notetaker` | Toggle, on |
| Notepad | `Split the screen when joining` | Toggle, off; requires accessibility access |
| Keyboard shortcut | `Triple tap {{dictation_key}} for Notetaker` | Toggle, on |
| Keyboard shortcut | Notetaker shortcut | Hotkey editor opened by `Change shortcut`; default binding uncertain |
| Transcript | `Show live transcript` | Toggle, on |
| Transcript | `Speaker names` | Toggle, on |
| Transcript | `Auto-detect speaker names` | Toggle, on; reads names from the meeting app |
| Sharing | `Notes sharing` or default note visibility | Dropdown: `Private`, `Shared with team`, `Anyone with the link`; default Anyone with the link, subject to organization policy |
| Sharing | `Calendar notice` | Dropdown: `Off` (default), `Notice only`, `Notice and notes link`; Google Calendar only in this build |
| Sharing | `Auto-share notes` | Audience dropdown: `no one` (default), `all attendees`, or same-domain attendees |
| Sharing | `Which meetings` | Scope dropdown: `all meetings` (default) or `meetings with a calendar notice` |
| Tutorial | `Start tutorial` | Button opening Notetaker education |
| Import | `Import your meeting notes` | Granola and Otter import actions; availability depends on onboarding answers and flags |

### Vibe Coding

| Setting | Type and default |
| --- | --- |
| `Variable recognition (VS Code, Cursor, Windsurf)` | `Set up` button. Instructions open the editor command palette, run `Toggle Screen Reader Accessibility Mode`, and confirm `Screen Reader Optimized` |
| `File Tagging in Chat (Cursor & Windsurf)` | Toggle, on. It tags files in the supported IDE chat context |

### Experimental

| Setting | Type and default |
| --- | --- |
| `Command Mode` | Toggle, off; hidden when Instruct Mode is supplied instead |
| `Press Enter command` | Toggle, off; presses Enter when the phrase is spoken at the end |
| `Stacked messages` | Toggle, off; splits one dictation into separate messages in messaging apps |
| `Style Detection` | Toggle, on; automatically chooses messaging style |
| `Bulk import` | Toggle, off; exposes file import for snippets and dictionary entries |
| `Jabra wear detection` | Toggle, off; switches to a supported worn Jabra microphone and back when removed |
| `Focus Mode` | Toggle, off and restricted to internal-email accounts; configuration lists `Blocked Apps` and `Blocked URLs` and has `+ Add app` and URL entry |
| `Email preferences` | Conditional Instruct Mode connector panel. Actions include `Connect Gmail`, `Learn from emails`, `View email profile`, `Regenerate preferences`, and `Disconnect` |

### Connectors

This page is headed `Connectors` and says `Connecting your apps helps Wispr Flow give you accurate reminders, summaries, and briefs`. Each connector card has a state/action such as `Connect`, `Connected`, `Reconnect`, `Disconnect`, `Try again`, or `Finish connecting`. Organization policy can make a card unavailable.

The base catalog includes Google Calendar for meeting reminders and briefs, Outlook for the same purpose, and Slack for message context. The newer connector catalog adds Gmail or Google Automations for email and calendar automation, Notion for documents and brief context, GitHub for tool integrations, and Linear for project-management integrations. Disconnect dialogs explain what context or briefs may be deleted. A banner can link to MCP with `Give your AI access to your meeting transcripts and notes`.

### MCP

The page title is `MCP`. Cards for `Claude` and `ChatGPT` use `Add to {{name}}`. A manual section titled `All other apps:` shows the server URL in a copy field. The three instructions are `Copy your connection URL`, add it as a custom connector or plugin, and approve access in the browser. There are no persistent MCP preference toggles on this page.

### Extensions

A separate settings-sidebar label for Extensions exists behind a feature flag, alongside the Hub Extensions page. Its visible settings and whether it redirects to the connector catalog are unresolved in this build `(uncertain)`.

### Account

| Setting | Control |
| --- | --- |
| `Profile picture` | Image upload, maximum 5 MB |
| `First name` | Text field, required |
| `Last name` | Text field, required |
| `Email` | Read-only account value |
| `Save` | Saves the profile fields and picture |
| `Sign out` | Confirmation; warns that recent local transcript history is deleted |
| `Delete account` | Typed confirmation; deletes account data, memory, and dictionary locally and on Wispr servers |

The bundle also contains a confirmation for `Delete all transcripts`, though its exact placement on the rendered Account page is conditional `(uncertain)`.

### Team or Organization

Users without a team see creation, invitation, domain discovery, join-request, or rejoin flows. Creation asks for a team name and teammate emails and may offer `Auto-add all future @{{domain}} users`. Existing teams show a member table with columns `Name`, `Role`, and `Status`, tabs for `Team members`, `Requests`, and `Other users on your domain`, and controls to add users, approve or deny requests, refresh, add billing information, and open `Admin portal`. Status values include Active, Pending, and trial end dates. Enterprise or SCIM-managed organizations move some membership controls into the identity provider or admin portal.

### Plans & Billing

The page shows the current plan, word allowance, renewal or cancellation date, seat counts, and subscription state. Actions include `Upgrade`, `Upgrade to Pro`, `Manage subscription`, `Manage billing`, `Add billing info`, `Update payment`, `Create a team`, `Contact sales`, and subscription refresh. Checkout supports `Monthly` and `Annual`, promo codes, payment summary, seat count, and bundle choice between `Dictation only` and `Dictation and unlimited Notetaker`.

Plan cards include `Free`, `Flow Pro`, `Flow Student`, `Growth`, and `Enterprise`, based on eligibility and team context. The card feature lists cover unlimited dictation, apps and languages, shared dictionary and snippets, Notetaker limits, Ask Wispr, connectors, usage analytics, admin tools, SSO, SCIM, domain capture, compliance, training controls, support, and commercial terms. Pricing values are fetched rather than fixed in the renderer, so this reference does not assign amounts.

### Data & Privacy

| Setting | Type, choices, and default |
| --- | --- | --- |
| `Improve the model for everyone` | Toggle; treated as on when unset. Allows audio, transcripts, and edits to help evaluate and improve Wispr models |
| `Dictation cloud storage` | Toggle, on. Stores transcript, audio, and history on Wispr servers for cross-device features; HIPAA or organization policy can lock it off |
| `Context awareness` | Toggle, on. Uses limited relevant text from the active app to improve names and interpretation |
| `Local data storage` | Dropdown: `Store data locally` (default), `Auto-delete local data every 24hrs`, `Never store data locally`; destructive changes require confirmation |
| `Notes sharing` | Dropdown: `Private`, `Shared with team`, `Anyone with the link`; default Anyone with the link and subject to policy |
| `Notetaker transcript retention` | Dropdown: `Never delete` (default), 365, 180, 90, 30, 7, or 1 day; older transcripts are deleted from devices and Wispr servers |
| `Hard refresh all notes` | `Sync notes` button; forces a one-time full cloud rescan |
| HIPAA BAA | Individual or organization controls to view, accept, download, manage, enable, or revoke a BAA; role and plan dependent |
| Data Controls | `Read about our Data Controls` link |

Organization policy can lock model improvement, cloud storage, retention, visibility, HIPAA, and connector behavior. The page labels locked values with `This setting is managed by your organization.`

### Internal

This page is restricted to Wispr accounts. It exposes diagnostic rather than normal customer preferences: data-sharing debug overrides, model and plan/status selectors, accessibility-tree capture, the Meeting AX Inspector, feature-flag refresh, sandbox-user controls, notification previews, onboarding and Notetaker resets, Bluetooth ring pairing and tests, synthetic meeting reminders, recording-length overrides, sync controls, and assorted experiment switches. These controls should not be treated as supported product settings.

### Testing

The feature-flagged Testing page can reset onboarding while preserving selected intent, reset personalization, override student state, trigger sync WebSocket events, and override or reset feature flags. It is a test surface, not a normal user page.

## Floating Flow Bar

### Position and shape

The stored dock edge defaults to bottom. Layout code supports bottom, left, and right docking and a movable, draggable Flow Bar. At rest it is a small rounded pill near the selected screen edge. Active states expand it to fit an audio visualization, action buttons, notices, meeting status, or transform results. The component recalculates the transparent window shape as its contents change. Users can drag it and open a three-dot context menu.

### Dictation states

The top-level state model contains hidden, resting, context-menu, initializing, ready, active push-to-talk, active hands-free, processing, polish processing, polish completed with action, polish completed without action, polish failed, Auto Cleanup completed, hotkey reminder, instruct completed, and error. The nested dictation state adds initializing, listening, stopping, processing, dismissed, retrying, error, idle, and testing.

Listening and processing are mainly conveyed by icon, animation, waveform, and shape rather than a persistent literal `Listening` or `Processing` label. Visible actions and notices include `Cancel`, `Finish and paste`, `Cleaned up!`, `See what’s changed`, `Open instruct chat`, `Change language`, `Scratchpad`, and `Voice-to-text disabled`. Related transient notices include `Taking longer than usual…`, `We polished your dictation`, `Using Command Mode`, `Dictating with Wispr Flow`, `No audio received`, `Select text to apply a transform`, `Flow can't paste right now. Text saved to clipboard`, and `Flow was processing your last transcript`.

The hotkey reminder appears when the app wants to remind the user to dictate rather than type. It can be configured by app category, hidden for an hour, or muted for 12 hours. Some reminders show the configured key rather than a fixed label.

### Meeting states

The meeting substate is idle, recording, reconnecting, flushing, or compressing. While recording, the bar can show the meeting title, elapsed state, and actions such as `Take notes`, `Show Notepad`, and `Stop Notetaker`. Reconnection reads `Still listening · Reconnecting…`; shutdown or finalization reads `Wrapping up…`. Calendar reminder pills can show an upcoming meeting, countdown, join-and-record action, snooze, start Notetaker, and notification settings.

### Context menu

The Flow Bar menu contains `Languages`, `Microphone`, conditional `Microphone (Notetaker)`, `Formatting options`, `Transcript history`, `Paste last transcript`, `Settings`, and reminder actions. When a reminder is active it also offers `Configure reminder`, `Hide for 1 hour`, and `Mute reminder for 12 hours`. The context-menu window also hosts `Fetch links`, which can present pinned links, recent links, no links, or no matching links, plus the transform diff headed `Cleaned up! Here’s what changed` with `Configure` and `Done`.

## Other windows

### Scratchpad window

The detachable Scratchpad has note tabs, `New tab`, `New note`, recent-note search, compact and expanded window modes, and a rich-text canvas. Its toolbars expose `Formatting`, `Transforms`, `Version history`, `Copy`, `Send`, and delete/close actions. Transform input says `Follow up or ask a question`, with `Get new suggestions` after a result. Version entries are labeled Created, Dictated, Typed edits, Transform, and Custom transform. Images can be dropped into the editor; tables and links have their own editing menus.

### Notepad and meeting-recorder window

The floating Notepad follows the active meeting and can be opened automatically when Notetaker starts. It combines editable private thoughts or notes with live meeting status and navigation to summary, transcript, tasks, brief, and documents. It can remain beside the conferencing app, participate in split-screen placement, and be hidden from screen capture when that setting is enabled. Recording continues through reconnecting and finalization states even if the window is hidden, subject to the stop and failure paths.

### Feature tour

The feature-tour renderer presents the onboarding and education cards outside the main Hub layout. The extracted copy covers introduction, sign-in, microphone and shortcut setup, language selection, dictation practice, profile questions, team creation and invitations, Notetaker recording habits, calendar connection, app connection, and plan or trial announcements. Navigation uses labels such as `Continue`, `Back`, `Skip`, `Skip for now`, `Start Setup`, and `Finish` depending on the step.

### Overlay

The overlay renderer is a very small transparent surface used for transient on-screen UI rather than a navigable window. The bundle does not contain enough distinct visible copy to identify a stable user workflow `(uncertain)`.

### Meeting AX Inspector

`Meeting AX Inspector` is an internal diagnostic window. It reports accessibility observations from Google Meet, Zoom, and Teams, microphone holders, meeting-window binding candidates, speaker-source events, helper restarts, and whether detection is supported or active. It has connection states such as `Connecting…`, `Retry`, and `Resume`. It is gated to internal users.

## Desktop integration and local behavior

Wispr Flow uses Electron IPC to coordinate the Hub, Flow Bar, context menu, Scratchpad, Notepad, tray, global shortcuts, clipboard insertion, authentication, recording, notifications, preferences, and updates. A native helper supplies global input, accessibility, active-window, selected-text, meeting-window, and device observations. Windows-specific code recognizes common browsers, meeting tools, messaging apps, and editors so formatting, reminders, and meeting detection can react to the foreground application.

Account sign-in opens a web login and uses a device-code-style server event flow before saving refreshed session credentials. Connector authorization and MCP approval also finish in the browser. The app keeps structured local data in a Sequelize SQLite database named `flow.sqlite`, including dictations, dictionary and snippet records, notes and versions, meetings, calendar events, and sync state. Storage and retention preferences determine which content remains locally or in the cloud.

The network layer talks to Wispr services for authentication, transcription, transforms, sync, billing, teams, meetings, connectors, feature flags, and updates. Sentry code is bundled for crash and error reporting. The app also contains product analytics and notification-event plumbing, but static packaging alone does not establish which events are enabled for a particular account. User-facing controls for model improvement and cloud storage are documented under Data & Privacy and should not be assumed to disable every operational diagnostic event `(uncertain)`.
