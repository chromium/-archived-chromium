/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DatabaseTableView = function(database, tableName)
{
    WebInspector.View.call(this);

    this.database = database;
    this.tableName = tableName;

    this.element.addStyleClass("storage-view");
    this.element.addStyleClass("table");
}

WebInspector.DatabaseTableView.prototype = {
    show: function(parentElement)
    {
        WebInspector.View.prototype.show.call(this, parentElement);
        this.update();
    },

    update: function()
    {
        function queryTransaction(tx)
        {
            tx.executeSql("SELECT * FROM " + this.tableName, null, InspectorController.wrapCallback(this._queryFinished.bind(this)), InspectorController.wrapCallback(this._queryError.bind(this)));
        }

        this.database.database.transaction(InspectorController.wrapCallback(queryTransaction.bind(this)), InspectorController.wrapCallback(this._queryError.bind(this)));
    },

    _queryFinished: function(tx, result)
    {
        this.element.removeChildren();

        var dataGrid = WebInspector.panels.databases.dataGridForResult(result);
        if (!dataGrid) {
            var emptyMsgElement = document.createElement("div");
            emptyMsgElement.className = "storage-table-empty";
            emptyMsgElement.textContent = WebInspector.UIString("The “%s”\ntable is empty.", this.tableName);
            this.element.appendChild(emptyMsgElement);
            return;
        }

        this.element.appendChild(dataGrid.element);
    },

    _queryError: function(tx, error)
    {
        this.element.removeChildren();

        var errorMsgElement = document.createElement("div");
        errorMsgElement.className = "storage-table-error";
        errorMsgElement.textContent = WebInspector.UIString("An error occurred trying to\nread the “%s” table.", this.tableName);
        this.element.appendChild(errorMsgElement);
    },

}

WebInspector.DatabaseTableView.prototype.__proto__ = WebInspector.View.prototype;
