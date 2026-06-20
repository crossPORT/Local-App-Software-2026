
|                                                                                                                                          |     |                                                                                                                 |
| ---------------------------------------------------------------------------------------------------------------------------------------- | --- | --------------------------------------------------------------------------------------------------------------- |
| **RocketBox Fabric File Transfer****Cross-Team File Exchange for Co-Located Systems***June 2026 · Confidential · Product Requirements* |     | **Creative · CAD · Accounting****USB switched fabric****No IP · No credentials · No IT****One activity log** |




  



|                                                                                                                                                                                                                                                                                                                                                                                                                  |
| ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Three co-located teams sit around one RocketBox and move files between one another.** A system joins the fabric the moment its cable is connected and its app is open. There is no IP address on the data path, no credentials, and no IT setup. This document defines the cross-team use cases, the way systems announce themselves, the send and receive workflow, and the information the app must present. |


  



|                           |     |                         |     |                       |     |                        |     |                           |
| ------------------------- | --- | ----------------------- | --- | --------------------- | --- | ---------------------- | --- | ------------------------- |
| **3**Teams on one fabric |     | **0**Credentials or IP |     | **0**IT provisioning |     | **5**Core app screens |     | **1**Shared activity log |


  


**Scope**


|                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |     |     |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --- | --- |
| This release covers file send and receive between host systems on a single RocketBox over the USB switched fabric. The three reference teams are a small Creative team, a CAD team, and an Accounting team, all co-located around one appliance.A system is reachable only while its cable is connected and its app is open. The physical cable is the access control. The app must be present on every connecting system, installed once by the user with no IT involvement. |     |   |


  
  


**Objectives and Non-Goals**

**Objectives**

- Let any system on the fabric send files or folders to any other present system in two steps: pick the recipient, send.
- Make presence obvious. A user should see who is on the fabric right now, grouped by team, without configuration.
- Protect sensitive receives. Nothing writes to a workstation that has asked to be prompted first.
- Produce a defensible record. Every transfer is logged on both sides with enough detail for an audit.

**Non-Goals**

- No identity directory, single sign-on, or central administration. Identity is asserted by the system itself in the app.
- No file versioning system. The activity log and an optional note give the sender certainty about what was delivered.
- No transfer to systems that are not physically connected and running the app.



**Users and the Three Teams**

The three teams hand work back and forth in patterns that are not symmetric. Creative and CAD trade large files freely and would auto-accept from each other. Accounting both sends and receives financially sensitive material that should never land silently. That single fact drives the accept-rule design.

  



|                        |                                                              |                 |
| ---------------------- | ------------------------------------------------------------ | --------------- |
| **From and to**        | **What moves**                                               | **Sensitivity** |
| Creative to CAD        | Reference art, brand assets, product imagery, texture files  | Low             |
| CAD to Creative        | Renders, exploded views, turntables for marketing            | Low             |
| CAD to Accounting      | Bills of materials, part counts, spec sheets for costing     | Medium          |
| Accounting to CAD      | Approved budgets, cost ceilings, vendor part numbers         | Medium          |
| Creative to Accounting | Project asset lists, deliverable manifests for invoicing     | Low             |
| Accounting to Creative | Purchase order numbers, billing references, signed estimates | Higher          |


  


**Use Cases**

**Creative team — large media handoff**

A designer finishes a packaging concept and needs layered source files and product photography to reach CAD so they can match real dimensions. Files are large: multi-gigabyte layered images, raw photo sets, video. The designer opens the app, sees the CAD systems present, drags the folder onto the target workstation, and the transfer runs at fabric speed with no upload step and no file-size ceiling.

**CAD team — version certainty**

An engineer produces a rendered assembly and an exploded view and pushes them to a named creative system. Separately, the engineer exports a bill of materials and sends it to Accounting for a cost rollup. The risk is that the receiver opens an older copy. The app removes that doubt: the sender attaches a short note such as "final export, supersedes Tuesday," and both sides see exactly what was sent, where it went, and that it arrived.

**Accounting team — sensitive receives and audit**

Accounting receives a bill of materials from CAD, builds a costed quote, and sends the approved budget back. It also sends purchase-order and billing references to Creative. Two requirements follow. Nothing writes to an accounting workstation without a person accepting it, and Accounting needs a record of what was sent and received, by whom and when. This is what turns convenient file transfer into something the team can defend in an audit.



**How Systems Announce Themselves**

Because there is no network and no IP, presence is asserted at the app layer rather than discovered by broadcast. Joining the fabric feels like walking into the room: the system appears on everyone's roster, named and ready.



*Figure 1. Presence and announce sequence when a system joins the fabric.*

**The model in four parts**

- Cable connected and app open equals membership. The fabric admits the system. Nothing else is required of the user.
- The system declares an identity: display name, team, role, and a receive-status of open, ask-first, or busy. It is set once in settings and reused on every join.
- Every present system sees the updated roster the moment another system joins or leaves.
- Leaving is unplugging the cable or closing the app. Presence ends at once and the roster reflects it within a second or two.

  



|                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Identity trust ceiling — a decision to confirm**The fabric guarantees that everyone on the roster is a genuine participant, present by physical cable. The human-readable identity, however, is self-asserted in settings. For a small co-located team this is acceptable, because the desks are visible. The recommendation is to lean on the accept-rules below so sensitive receives always involve a person, rather than building a heavy identity-verification layer at this scale. |




**End-to-End Workflow**


|                                                                                                                                                                                                                                                                                       |     |                                                                                                                                                                                                                                                                                         |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Sending**Open the app, see the roster grouped by team, drag a file or folder onto a target system or select the target and pick files. Optionally add a one-line note. Send. Progress shows live, and on completion the sender gets a confirmation tied to that specific transfer. |     | **Receiving**Two paths by receive-status. Open systems auto-write the file to a designated receive folder and notify the user, which is right for bulk creative work. Ask-first systems show an incoming prompt with sender, file, and size, and write nothing until the user accepts. |




*Figure 2. Send and receive flow, including the ask-first branch and the shared activity log.*

After every transfer, both sides record the event: file name, size, sender, recipient, timestamp, and status of delivered, declined, or failed. This is the version certainty CAD needs and the audit trail Accounting needs.



**App Information Architecture**

The app supports the RocketBox Fabric on every connecting system. Five core screens carry the workflow. The roster is home; everything else is one tap away.



*Figure 3. The five core screens. Layout is indicative, not final visual design.*

**1. Presence roster (home)**

**Shows:** every system on the fabric now, grouped by team, each with display name, team, role, online status, and receive-status. If a system is not on the roster, it is not reachable, and that is made obvious rather than confusing. The user's own identity and status sit at the foot of the screen.

**2. Send**

**Shows:** target system, a file and folder picker with drag-and-drop, an optional one-line note, live progress with speed and time remaining, and a cancel control.

**3. Incoming (ask-first)**

**Shows:** a pending incoming transfer with sender, file, size, and any note, plus accept and decline. Auto-accepted files instead surface as a notification. Nothing writes to disk until the user accepts.

**4. Activity log**

**Shows:** a chronological record of sent and received transfers with sender, recipient, file, size, timestamp, and status, filterable by team and by system. For Accounting this is the compliance feature; for everyone it answers whether a transfer went through and where the file landed.

**5. Identity and settings**

**Shows:** display name, team, role, default receive-status, the receive folder, and per-team accept rules such as auto-accept from CAD and Creative while always asking from Accounting.

  


**Functional Requirements**

**FR-1**  On cable connection and app launch, the system joins the fabric and announces its stored identity without any further user action.

**FR-2**  The roster updates for all present systems within two seconds of any system joining or leaving.

**FR-3**  A user can send one or more files, or a folder, to any single present system. Folders transfer as folders, not as a zip step the user must perform.

**FR-4**  A sender can attach a single-line note to a transfer.

**FR-5**  Receive-status of open writes incoming files to the receive folder and posts a notification. Receive-status of ask-first holds the file and prompts for accept or decline before any write.

**FR-6**  Accept rules can be set per team, with a global default, and override the per-transfer prompt where configured.

**FR-7**  Every transfer writes a log entry on both sender and recipient: file, size, counterpart, timestamp, and status.

**FR-8**  The activity log is filterable by team and by system.

**FR-9**  Closing the app or disconnecting the cable removes the system from all rosters immediately and cancels any in-flight transfer cleanly.

**FR-10**  No transfer path, presence path, or identity depends on an IP address, credential, certificate, or IT-managed policy.

**Success Metrics**


|                                |                                                                  |                                                 |
| ------------------------------ | ---------------------------------------------------------------- | ----------------------------------------------- |
| **Metric**                     | **Target**                                                       | **Why it matters**                              |
| Time to first send             | Under 60 seconds from app launch by a new user                   | Proves the no-setup promise holds in practice   |
| Roster accuracy                | Presence reflects reality within 2 seconds                       | A stale roster breaks trust in who is reachable |
| Misdirected sensitive receives | Zero silent writes to ask-first systems                          | The whole accounting case rests on this         |
| Log completeness               | 100% of transfers logged on both sides                           | An audit trail with gaps is not an audit trail  |
| Transfer success rate          | Over 99% of started transfers complete or report a clear failure | Silent failure is worse than visible failure    |


  


**Appendix — Terminology**

**System.**  A connected computer participating in the fabric, with its own compute, memory, and storage. Preferred over "device," which understates its role.

**RocketBox.**  The appliance and platform that switches the fabric and hosts presence. Not a network appliance, not a NAS, not a cloud service.

**USB switched fabric.**  The data path between systems. It works with every USB standard since 1996 and does not depend on USB-C.

**Receive-status.**  A system's standing instruction for incoming transfers: open, ask-first, or busy.

**Membership.**  The state of being on the fabric, established by a connected cable plus an open app, with the one-time app install already done by the user.

  


**Private working notes**

*Not for distribution. Open product decisions and rationale for sign-off before build.*

**1. Accept-rule granularity**

Recommendation: per-team rules with a global override. "Auto-accept CAD, ask everyone else" is the real pattern; per-system is too fiddly for a team this size.

**2. Log retention and export**

Accounting will ask to export the activity log. Decide now whether the first release only displays it or also exports, since it affects the data model.

**3. Busy status behavior**

Define what "busy" does to an incoming send: queue, bounce with a message, or block. Leaning toward queue with sender notice.