import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 940; height: 690; minimumWidth: 360; minimumHeight: 400
    visible: true; title: "Tally"; color: "#111214"
    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius
    property color canvas: "#101113"
    property color topSurface: "#191a1d"
    property color raisedSurface: "#202125"
    property color divider: "#2b2c31"
    property color treeGuide: "#3a3b40"
    property color primaryText: "#f1f0eb"
    property color secondaryText: "#a0a1a6"
    property bool reducedMotion: Qt.styleHints.accessibility.motionPreference === 1
    property int taskMotionDuration: reducedMotion ? 0 : 190
    property string selected: backend.currentListId
    property var activeList: { for (let i=0;i<backend.lists.length;i++) if (backend.lists[i].id===selected) return backend.lists[i]; return ({name:"Inbox",tasks:[]}) }
    property bool narrow: width < 660
    property string addParent: ""
    property bool helpVisible: false
    property bool undoBannerVisible: false
    property var activeTaskEditor: null
    property var collapsedTaskIds: []
    onSelectedChanged: Qt.callLater(function(){win.refreshListTitle()})
    Component.onCompleted: syncTaskModel()
    onClosing: function(close) { close.accepted = true }
    ListModel { id:taskModel; dynamicRoles:true }
    Connections {
        target:backend
        function onChanged() {
            Qt.callLater(function(){win.syncTaskModel();win.refreshListTitle()})
        }
        function onUndoChanged() {
            if(backend.undoAvailable) {
                win.undoBannerVisible=true
                undoTimer.restart()
            } else {
                win.undoBannerVisible=false
                undoTimer.stop()
            }
        }
    }
    Timer { id:undoTimer;interval:5000;onTriggered:win.undoBannerVisible=false }
    Shortcut { sequence: "Ctrl+N"; context: Qt.ApplicationShortcut; onActivated: taskEntry.forceActiveFocus() }
    Shortcut { sequence: "Ctrl+Shift+N"; context: Qt.ApplicationShortcut; onActivated: createList() }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: helpVisible=!helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: { if (taskEntry.activeFocus) { taskEntry.accepted() } else taskEntry.forceActiveFocus() } }

    function createList() { listDialog.editId=""; listDialog.nameField.text=""; listDialog.open(); listDialog.nameField.forceActiveFocus() }
    function addChild(id) { addParent=id; childDialog.field.text=""; childDialog.open() }
    function taskCollapsed(id) { return collapsedTaskIds.indexOf(id)>=0 }
    function toggleTaskCollapsed(id) {
        const ids=collapsedTaskIds.slice()
        const index=ids.indexOf(id)
        if(index>=0)ids.splice(index,1);else ids.push(id)
        collapsedTaskIds=ids
        syncTaskModel()
    }
    function flatten(tasks, depth, arr, doneState, ignoreCollapsed) {
        for(let i=0;i<tasks.length;i++) {
            if(doneState===null || tasks[i].done===doneState) arr.push({item:tasks[i],depth:depth})
            if(ignoreCollapsed||collapsedTaskIds.indexOf(tasks[i].id)<0)
                flatten(tasks[i].children||[],depth+1,arr,doneState,ignoreCollapsed)
        }
        return arr
    }
    function tasksFlat(doneState) { return flatten(activeList.tasks||[],0,[],doneState) }
    function allTasksFlat() { return flatten(activeList.tasks||[],0,[],null,true) }
    function refreshListTitle() {
        if(!listTitle||listTitle.editing)return
        for(let i=0;i<backend.lists.length;i++) {
            if(backend.lists[i].id===selected) {
                listTitle.text=backend.lists[i].name||"Inbox"
                return
            }
        }
    }
    function syncTaskModel() {
        const open=tasksFlat(false), done=tasksFlat(true), desired=[]
        for(let i=0;i<open.length;i++)desired.push({key:open[i].item.id,kind:"task",task:open[i].item,depth:open[i].depth})
        if(done.length>0)desired.push({key:"__done_divider__",kind:"divider",task:{},depth:0})
        for(let i=0;i<done.length;i++)desired.push({key:done[i].item.id,kind:"task",task:done[i].item,depth:done[i].depth})
        if(desired.length===0)desired.push({key:"__empty__",kind:"empty",task:{},depth:0})

        for(let i=taskModel.count-1;i>=0;i--) {
            let found=false
            for(let j=0;j<desired.length;j++)if(taskModel.get(i).key===desired[j].key){found=true;break}
            if(!found)taskModel.remove(i)
        }
        for(let i=0;i<desired.length;i++) {
            let at=-1
            for(let j=i;j<taskModel.count;j++)if(taskModel.get(j).key===desired[i].key){at=j;break}
            if(at<0)taskModel.insert(i,desired[i])
            else if(at!==i)taskModel.move(at,i,1)
            taskModel.setProperty(i,"kind",desired[i].kind)
            taskModel.setProperty(i,"task",desired[i].task)
            taskModel.setProperty(i,"depth",desired[i].depth)
        }
    }
    function finishEditOutside(x,y) {
        if (!activeTaskEditor) return
        const origin = activeTaskEditor.mapToItem(contentSurface,0,0)
        const inside = x >= origin.x && x <= origin.x + activeTaskEditor.width
                   && y >= origin.y && y <= origin.y + activeTaskEditor.height
        if (!inside) activeTaskEditor.focus=false
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Rectangle { Layout.fillWidth:true; Layout.preferredHeight:58; color:topSurface
            RowLayout { anchors.fill:parent; anchors.leftMargin:20; anchors.rightMargin:18; spacing:10
                Text { text:"T"; color:theme.accent; font.pixelSize:21; font.weight:Font.Black }
                Text { text:"TALLY"; color:primaryText; font.pixelSize:12; font.letterSpacing:2.3; font.weight:Font.DemiBold }
                ComboBox {
                    id:listPicker
                    Layout.fillWidth:true
                    Layout.minimumWidth:90
                    Layout.preferredWidth: narrow?115:205
                    Layout.maximumWidth:250
                    Layout.preferredHeight:36
                    model:backend.lists
                    textRole:"name"
                    currentIndex:Math.max(0,backend.lists.findIndex(x=>x.id===selected))
                    onActivated: function(index) {
                        const chosen=backend.lists[index]
                        selected=chosen.id
                        backend.selectList(chosen.id)
                        Qt.callLater(function(){win.refreshListTitle()})
                    }
                    delegate:ItemDelegate {
                        width:listPicker.width
                        height:34
                        padding:0;leftPadding:12;rightPadding:12
                        highlighted:listPicker.highlightedIndex===index
                        contentItem:Text {
                            text:listPicker.textAt(index)
                            color:parent.highlighted?primaryText:secondaryText
                            font.pixelSize:12
                            elide:Text.ElideRight
                            verticalAlignment:Text.AlignVCenter
                        }
                        background:Rectangle {
                            anchors.fill:parent
                            anchors.margins:4
                            radius:Math.min(4,theme.radius)
                            color:parent.highlighted?"#303128":parent.hovered?"#26272b":"transparent"
                        }
                    }
                    popup:Popup {
                        y:listPicker.height+4
                        width:listPicker.width
                        padding:4
                        implicitHeight:Math.min(contentItem.implicitHeight+padding*2,280)
                        contentItem:ListView {
                            clip:true
                            implicitHeight:Math.min(contentHeight,272)
                            model:listPicker.popup.visible?listPicker.delegateModel:null
                            currentIndex:listPicker.highlightedIndex
                            boundsBehavior:Flickable.StopAtBounds
                        }
                        background:Rectangle { color:raisedSurface;border.width:1;border.color:divider;radius:Math.min(6,theme.radius) }
                    }
                    background: Rectangle { color:raisedSurface; radius:Math.min(5,theme.radius); border.width:listPicker.popup.visible?1:0; border.color:theme.accent }
                    contentItem: Text { leftPadding:10; rightPadding:26; text:listPicker.displayText; color:"#dad9d5"; font.pixelSize:12; elide:Text.ElideRight; verticalAlignment:Text.AlignVCenter }
                    indicator: Text { x:listPicker.width-width-9; y:(listPicker.height-height)/2; text:"⌄"; color:"#9b9da3"; font.pixelSize:13 }
                }
                ToolButton {
                    text:"+"
                    onClicked:createList()
                    implicitWidth:36
                    implicitHeight:36
                    Accessible.name:qsTr("New list")
                    background:Rectangle { radius:Math.min(5,theme.radius); color:parent.hovered?"#2b2c30":"transparent" }
                    contentItem:Text{text:"+";color:theme.accent;font.pixelSize:20;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
                    ToolTip.visible:hovered
                    ToolTip.text:qsTr("New list")
                }
                Item { Layout.fillWidth:true }
                Text { text: qsTr("%1 open").arg(activeList.count||0); color:secondaryText; font.pixelSize:11 }
                Rectangle { width:1;height:20;color:divider }
                ToolButton { text:"?"; onClicked:helpVisible=true; contentItem:Text { text:parent.text;color:"#a8a9ad";horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;font.pixelSize:14 } }
            }
        }
        RowLayout { Layout.fillWidth:true; Layout.fillHeight:true; spacing:0
            Rectangle { id:contentSurface; Layout.fillWidth:true; Layout.fillHeight:true; color:canvas
                TapHandler {
                    acceptedButtons:Qt.LeftButton
                    onPressedChanged:if(pressed)win.finishEditOutside(point.position.x,point.position.y)
                }
                ColumnLayout { anchors.fill:parent; anchors.leftMargin:narrow?12:28;anchors.rightMargin:narrow?12:28;anchors.topMargin:16;anchors.bottomMargin:12;spacing:10
                    RowLayout { Layout.fillWidth:true; spacing:10
                        TextField {
                            id:listTitle
                            Layout.fillWidth:true
                            text:activeList.name||"Inbox"
                            property bool editing:false
                            readOnly:!editing
                            selectByMouse:editing
                            leftPadding:0; rightPadding:0; topPadding:0; bottomPadding:0
                            color:"#f2f1ec"
                            font.pixelSize:24; font.weight:Font.DemiBold
                            background:Item{}
                            onAccepted:{backend.renameList(selected,text);editing=false;focus=false}
                            onActiveFocusChanged:if(!activeFocus&&editing){backend.renameList(selected,text);editing=false}
                            TapHandler { onTapped: { listTitle.editing=true; listTitle.readOnly=false; listTitle.forceActiveFocus(); listTitle.selectAll() } }
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Edit list name")
                        }
                        ToolButton {
                            text:"···"
                            onClicked: { listContext.listId=selected; listContext.popup() }
                            contentItem:Text{text:parent.text;color:"#898b91";font.pixelSize:18;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
                        }
                    }
                    Rectangle { Layout.fillWidth:true; Layout.preferredHeight:42; radius:Math.min(5,theme.radius);color:topSurface;border.width:taskEntry.activeFocus?1:0;border.color:theme.accent
                        Item { anchors.fill:parent;anchors.leftMargin:14;anchors.rightMargin:14
                            TextField {
                                id:taskEntry
                                anchors.fill:parent
                                placeholderText:""
                                font.pixelSize:12;color:"#eeede8"
                                leftPadding:6;rightPadding:4
                                background:Item{}
                                selectByMouse:true
                                onAccepted:{backend.addTask(text,"");text="";forceActiveFocus()}
                                Keys.onPressed:function(event){if((event.modifiers&Qt.ControlModifier)&&event.key===Qt.Key_V){event.accepted=false}}
                            }
                            Text {
                                anchors.left:parent.left;anchors.leftMargin:6
                                anchors.verticalCenter:parent.verticalCenter
                                text:qsTr("Add a task")
                                color:"#85868c";font.pixelSize:12
                                visible:taskEntry.text.length===0&&!taskEntry.activeFocus
                            }
                        }
                    }
                    Text { visible:backend.error.length>0;text:backend.error;color:"#ff837b";font.pixelSize:11 }
                    ListView {
                        id:taskScroll
                        Layout.fillWidth:true;Layout.fillHeight:true
                        clip:true;spacing:0;model:taskModel
                        boundsBehavior:Flickable.StopAtBounds
                        move:Transition {
                            NumberAnimation { properties:"y";duration:win.taskMotionDuration;easing.type:Easing.OutCubic }
                        }
                        displaced:Transition {
                            NumberAnimation { properties:"y";duration:win.taskMotionDuration;easing.type:Easing.OutCubic }
                        }
                        add:Transition {
                            ParallelAnimation {
                                NumberAnimation { property:"opacity";from:0.72;to:1;duration:win.taskMotionDuration;easing.type:Easing.OutCubic }
                                NumberAnimation { property:"scale";from:0.98;to:1;duration:win.taskMotionDuration;easing.type:Easing.OutCubic }
                            }
                        }
                        remove:Transition {
                            NumberAnimation { property:"opacity";to:0;duration:Math.min(120,win.taskMotionDuration);easing.type:Easing.InCubic }
                        }
                        delegate: Item {
                            id:taskDelegate
                            required property string key
                            required property string kind
                            required property var task
                            required property int depth
                            width:taskScroll.width
                            height:kind==="task"?taskItem.height:kind==="divider"?17:72
                            TaskRow { id:taskItem;visible:taskDelegate.kind==="task";width:taskDelegate.width;task:taskDelegate.task;depth:taskDelegate.depth }
                            Rectangle { visible:taskDelegate.kind==="divider";x:5;y:8;width:Math.max(0,parent.width-10);height:1;color:divider }
                            Column {
                                visible:taskDelegate.kind==="empty";y:42;width:parent.width;spacing:6
                                Text { text:qsTr("Nothing on this list yet");color:primaryText;font.pixelSize:14;font.weight:Font.Medium;anchors.horizontalCenter:parent.horizontalCenter }
                                Text { text:qsTr("Add a task above or paste something in.");color:secondaryText;font.pixelSize:12;anchors.horizontalCenter:parent.horizontalCenter }
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth:true
                        Layout.preferredHeight:undoBannerVisible?42:0
                        visible:undoBannerVisible
                        radius:Math.min(5,theme.radius)
                        color:raisedSurface
                        border.width:1;border.color:divider
                        RowLayout {
                            anchors.fill:parent;anchors.leftMargin:12;anchors.rightMargin:6;spacing:8
                            Text { text:qsTr("Task deleted");color:primaryText;font.pixelSize:12;Layout.fillWidth:true }
                            ToolButton {
                                text:qsTr("Undo")
                                implicitWidth:54;implicitHeight:30
                                onClicked:backend.undo()
                                contentItem:Text { text:parent.text;color:theme.accent;font.pixelSize:12;font.weight:Font.DemiBold;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter }
                            }
                            ToolButton {
                                text:"×"
                                implicitWidth:28;implicitHeight:30
                                onClicked:undoBannerVisible=false
                                Accessible.name:qsTr("Dismiss undo message")
                                contentItem:Text { text:parent.text;color:secondaryText;font.pixelSize:15;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter }
                            }
                        }
                    }
                    RowLayout { Layout.fillWidth:true;spacing:8
                        Text { text:qsTr("%1 tasks").arg(allTasksFlat().length);color:secondaryText;font.pixelSize:10;Layout.fillWidth:true }
                        Text { text:qsTr("Ctrl+N to add");color:"#74767c";font.pixelSize:10 }
                    }
                }
            }
        }
    }

    component TaskRow: Item {
        id: row
        property var task
        property int depth:0
        property int visualDepth:Math.min(depth,5)
        property real indentation:Math.min(depth,5)*14+5
        height:Math.max(30,line.implicitHeight+4)
        Behavior on indentation {
            NumberAnimation { duration:win.taskMotionDuration;easing.type:Easing.OutCubic }
        }
        Rectangle { anchors.fill:parent;radius:Math.min(4,theme.radius);color:rowHover.hovered?"#1a1b1f":"transparent" }
        HoverHandler { id:rowHover }
        Repeater {
            model:visualDepth
            delegate: Rectangle {
                required property int index
                x:10+index*14
                width:1
                anchors.top:parent.top
                anchors.bottom:parent.bottom
                color:treeGuide
                opacity:0.78
            }
        }
        Rectangle {
            visible:visualDepth>0
            x:(visualDepth-1)*14+10
            y:line.y+line.height/2
            width:Math.max(0,visualDepth*14+5-x)
            height:1
            color:treeGuide
            opacity:0.9
        }
        RowLayout { id:line; anchors.left:parent.left;anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;anchors.leftMargin:row.indentation;anchors.rightMargin:4;spacing:6
            ToolButton {
                text:(task.children||[]).length>0?(win.taskCollapsed(task.id)?"›":"⌄"):""
                implicitWidth:18;implicitHeight:24
                enabled:(task.children||[]).length>0
                onClicked:win.toggleTaskCollapsed(task.id)
                Accessible.name:win.taskCollapsed(task.id)?qsTr("Expand subtasks"):qsTr("Collapse subtasks")
                ToolTip.visible:hovered&&enabled
                ToolTip.text:win.taskCollapsed(task.id)?qsTr("Expand subtasks"):qsTr("Collapse subtasks")
                background:Rectangle { radius:Math.min(4,theme.radius);color:parent.hovered&&parent.enabled?"#2b2c30":"transparent" }
                contentItem:Text { text:parent.text;color:parent.enabled?(parent.hovered?theme.accent:"#8e9096"):"transparent";font.pixelSize:14;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter }
            }
            Rectangle { width:16;height:16;radius:4;color:task.done?theme.accent:"transparent";border.width:1;border.color:task.done?theme.accent:"#686a71"
                Text { anchors.centerIn:parent;text:"✓";visible:task.done;color:"#111214";font.pixelSize:10;font.bold:true }
                MouseArea { anchors.fill:parent;onClicked:backend.setDone(task.id,!task.done) }
            }
            TextArea { id:edit; Layout.fillWidth:true;Layout.minimumWidth:70;Layout.preferredHeight:Math.max(22,contentHeight+2);text:task.text;readOnly:!editing;property bool editing:false;color:task.done?"#676970":"#deddd8";font.pixelSize:12;font.strikeout:task.done;wrapMode:TextEdit.WordWrap;selectByMouse:true;leftPadding:0;rightPadding:0;topPadding:2;bottomPadding:2;background:Rectangle{color:edit.editing?"#1c1d20":"transparent";radius:3}
                function finishEdit(){
                    if(!editing)return
                    backend.setTaskText(task.id,text)
                    editing=false;readOnly=true
                    if(win.activeTaskEditor===edit)win.activeTaskEditor=null
                }
                Keys.onReturnPressed:function(event){if(event.modifiers&Qt.ControlModifier){finishEdit();focus=false}else event.accepted=false}
                TapHandler { onDoubleTapped:{edit.editing=true;edit.readOnly=false;edit.forceActiveFocus();edit.selectAll();win.activeTaskEditor=edit} }
                onActiveFocusChanged:if(!activeFocus&&editing)finishEdit()
                Component.onCompleted:readOnly=true
            }
            ToolButton {
                text:"↑"; implicitWidth:22; implicitHeight:26
                visible:!task.done
                enabled:backend.lists.length>=0&&backend.canMoveTaskUp(task.id)
                onClicked:backend.moveTaskUp(task.id)
                Accessible.name:depth>0?qsTr("Move out above the parent"):backend.canIndentTaskUnderPrevious(task.id)?qsTr("Nest under the previous task"):qsTr("Move task up")
                ToolTip.visible:hovered; ToolTip.text:depth>0?qsTr("Move out above the parent"):backend.canIndentTaskUnderPrevious(task.id)?qsTr("Nest under the previous task"):qsTr("Move task up")
                background:Rectangle{radius:Math.min(4,theme.radius);color:parent.hovered&&parent.enabled?"#2b2c30":"transparent"}
                contentItem:Text{text:parent.text;color:parent.enabled?(parent.hovered?theme.accent:"#a0a1a6"):"#484a4f";font.pixelSize:14;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
            }
            ToolButton {
                text:"↓"; implicitWidth:22; implicitHeight:26
                visible:!task.done
                enabled:backend.lists.length>=0&&backend.canMoveTaskDown(task.id,win.collapsedTaskIds)
                onClicked:backend.moveTaskDown(task.id,win.collapsedTaskIds)
                Accessible.name:qsTr("Move task down one row")
                ToolTip.visible:hovered; ToolTip.text:qsTr("Move task down one row")
                background:Rectangle{radius:Math.min(4,theme.radius);color:parent.hovered&&parent.enabled?"#2b2c30":"transparent"}
                contentItem:Text{text:parent.text;color:parent.enabled?(parent.hovered?theme.accent:"#a0a1a6"):"#484a4f";font.pixelSize:14;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
            }
            ToolButton {
                text:"+"; implicitWidth:26; implicitHeight:26
                onClicked:addChild(task.id)
                visible:!task.done
                enabled:depth<2&&(task.children||[]).length<2
                Accessible.name:qsTr("Add subtask")
                ToolTip.visible:hovered;ToolTip.text:enabled?qsTr("Add subtask"):qsTr("Maximum nesting or subtasks reached")
                background:Rectangle{radius:Math.min(4,theme.radius);color:parent.hovered?"#2b2c30":"transparent"}
                contentItem:Text{text:"+";color:parent.enabled?(parent.hovered?theme.accent:"#8e9096"):"#484a4f";font.pixelSize:16;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
            }
            ToolButton {
                text:"×"; implicitWidth:26; implicitHeight:26
                onClicked:backend.removeTask(task.id)
                Accessible.name:qsTr("Delete task")
                background:Rectangle{radius:Math.min(4,theme.radius);color:parent.hovered?"#382324":"transparent"}
                contentItem:Text{text:"×";color:parent.hovered?"#ff8580":"#777980";font.pixelSize:16;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
            }
        }
    }
    Menu {
        id:listContext
        property string listId:""
        padding:4
        background:Rectangle { implicitWidth:190;color:raisedSurface;border.width:1;border.color:divider;radius:Math.min(6,theme.radius) }
        MenuItem {
            text:qsTr("Rename list")
            implicitWidth:190;implicitHeight:34
            padding:0;leftPadding:12;rightPadding:12
            background:Rectangle { anchors.fill:parent;anchors.margins:4;radius:Math.min(4,theme.radius);color:parent.highlighted?"#303128":parent.hovered?"#26272b":"transparent" }
            onTriggered:{listDialog.editId=listContext.listId;listDialog.nameField.text="";for(let i=0;i<backend.lists.length;i++)if(backend.lists[i].id===listContext.listId)listDialog.nameField.text=backend.lists[i].name;listDialog.open();listDialog.nameField.forceActiveFocus()}
        }
        MenuItem {
            text:qsTr("Delete list")
            implicitWidth:190;implicitHeight:34
            padding:0;leftPadding:12;rightPadding:12
            background:Rectangle { anchors.fill:parent;anchors.margins:4;radius:Math.min(4,theme.radius);color:parent.highlighted?"#303128":parent.hovered?"#26272b":"transparent" }
            enabled:backend.lists.length>1
            onTriggered:confirmDelete.open()
        }
    }
    Dialog { id:listDialog; property string editId:""; property alias nameField:field; modal:true;title:editId?qsTr("Rename list"):qsTr("New list");standardButtons:Dialog.Ok|Dialog.Cancel;anchors.centerIn:parent;width:Math.min(parent.width-28,330)
        contentItem:TextField{id:field;placeholderText:qsTr("List name");selectByMouse:true;onAccepted:listDialog.accept()}
        onAccepted:{if(editId)backend.renameList(editId,nameField.text);else{backend.addList(nameField.text);selected=backend.currentListId}}
    }
    Dialog { id:childDialog;property alias field:childField;modal:true;title:qsTr("Add subtask");standardButtons:Dialog.Ok|Dialog.Cancel;anchors.centerIn:parent;width:Math.min(parent.width-28,330)
        contentItem:TextField{id:childField;placeholderText:qsTr("Subtask");selectByMouse:true;onAccepted:childDialog.accept()}
        onAccepted:backend.addTask(childField.text,addParent)
    }
    Dialog { id:confirmDelete;modal:true;title:qsTr("Delete this list?");standardButtons:Dialog.Yes|Dialog.No;anchors.centerIn:parent;width:Math.min(parent.width-28,330);onAccepted:backend.removeList(listContext.listId) }
    Dialog { id:helpDialog;modal:true;title:qsTr("Shortcuts");standardButtons:Dialog.Close;anchors.centerIn:parent;visible:helpVisible;onClosed:helpVisible=false;width:Math.min(parent.width-28,360)
        contentItem:Column {
            spacing:8
            Label { text:qsTr("Ctrl+N     Add a task") }
            Label { text:qsTr("Ctrl+Shift+N     New list") }
            Label { text:qsTr("Double click a task to edit") }
            Label { text:qsTr("＋     Add a nested subtask") }
            Label { text:qsTr("↑     Promote a subtask or nest under the previous task") }
            Label { text:qsTr("↓     Nest a task or move a subtask out") }
            Label { text:qsTr("Right click a list to rename or delete") }
        }
    }
    onHelpVisibleChanged:if(helpVisible)helpDialog.open()
}
