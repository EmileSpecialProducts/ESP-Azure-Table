/**
 * Create a Table from a Json structure array.
 * first line will be used to gat the colom names
 * 
 * @param {*} tableDivId Name of the Div  
 * @param {*} Json structure array 
 * @param {*} Selector add a selector true is default
 * 
 */
function createTable(tableDivId, Json, Selector = true) {
    const Tablename = Object.keys(Json);
    const TableColoms = Object.keys(Json[Tablename][0]);
    const Table = Json[Tablename];
    // Get all the colums names  
    for (var l = 0; l < Json[Tablename].length; l++) {
        const Cols =Object.keys(Json[Tablename][l])
        for (var i = 0; i < Cols.length; i++) {
            TableColoms.indexOf(Cols[i]) === -1 ? TableColoms.push(Cols[i]) : {} };
    }     
    var table = document.createElement("table") // Create table header.
    table.classList.add("table-sortable");
    table.classList.add("dc_table_s1");
    var header = table.createTHead()
    for (var i = 0; i < TableColoms.length; i++) {
        var th = document.createElement("th"); // Table header.
        th.innerHTML = TableColoms[i];
        if (Selector == true) {
            var input = document.createElement("input");
            input.type = "text";
            input.className = "css-class-name"; // set the CSS class
            th.appendChild(input); // put it into the DOM
        }
        header.appendChild(th);
    }

    for (var i = 0; i < Table.length; i++) {
        tr = table.insertRow(-1); // add new row for the names
        for (var c = 0; c < TableColoms.length; c++) {
            td = tr.insertCell(-1);
            td.innerHTML = Table[i][TableColoms[c]] === undefined ? "" : Table[i][TableColoms[c]];
        }
    }

    var divContainer = document.getElementById(tableDivId);
    divContainer.innerHTML = "";
    divContainer.appendChild(table);

    if (Selector == true) {
        divContainer.querySelectorAll(".table-sortable th input").forEach(InputCell => {
            InputCell.addEventListener("keyup", () => {
                const tableElement = InputCell.parentElement.parentElement.parentElement;
                selectTableByColumn(tableElement);
            });
        });
    }

    divContainer.querySelectorAll(".table-sortable th").forEach(headerCell => {
        headerCell.addEventListener("click", () => {
            const tableElement = headerCell.parentElement.parentElement;//.parentElement;
            const headerIndex = Array.prototype.indexOf.call(headerCell.parentElement.children, headerCell);
            const currentIsAscending = headerCell.classList.contains("th-sort-asc");
            sortTableByColumn(tableElement, headerIndex, !currentIsAscending);
        });
    });

}

/**
 * Sorts a HTML table.
 *
 * @param {HTMLTableElement} table The table to sort
 * @param {number} column The index of the column to sort
 * @param {boolean} asc Determines if the sorting will be in ascending
 */

function sortTableByColumn(table, column, asc = true) {
    const dirModifier = asc ? 1 : -1;
    const tBody = table.tBodies[0];
    const rows = Array.from(tBody.querySelectorAll("tr"));

    // Sort each row
    const sortedRows = rows.sort((a, b) => {
        const aColText = a.querySelector(`td:nth-child(${column + 1})`).textContent.trim();
        const bColText = b.querySelector(`td:nth-child(${column + 1})`).textContent.trim();

        return aColText > bColText ? (1 * dirModifier) : (-1 * dirModifier);
    });

    // Remove all existing TRs from the table
    while (tBody.firstChild) {
        tBody.removeChild(tBody.firstChild);
    }

    // Re-add the newly sorted rows
    tBody.append(...sortedRows);

    // Remember how the column is currently sorted
    table.querySelectorAll("th").forEach(th => th.classList.remove("th-sort-asc", "th-sort-desc"));
    table.querySelector(`th:nth-child(${column + 1})`).classList.toggle("th-sort-asc", asc);
    table.querySelector(`th:nth-child(${column + 1})`).classList.toggle("th-sort-desc", !asc);
}

/**
 * Will hide non selected rows
 * @param {*} table 
 */
function selectTableByColumn(table) {
    const thead = table.tHead;
    const coloms = Array.from(thead.querySelectorAll("input"));
    const tBody = table.tBodies[0];
    const rows = Array.from(tBody.querySelectorAll("tr"));
    rows.forEach(row => {
        let show = true;
        for (var i = 0; i < coloms.length; i++) {
            if (coloms[i].value.length > 0) {
                if (row.childNodes[i].innerText.indexOf(coloms[i].value) < 0) {
                    show = false;
                }
            }
        }
        if (show == false)
            row.style.display = "none";
        else
            row.style.display = "";
    });
}


// Create an HTML table using the JSON data.
function createTableFromJSON(jsonData) {
    var arrBirds = [];
    arrBirds = JSON.parse(jsonData); // Convert JSON to array.
    var col = [];
    for (var key in arrBirds) {
        if (col.indexOf(key) === -1) {
            col.push(key);
        }
    }
    // Create a dynamic table.
    var table = document.createElement("table") // Create table header.
    var thead = document.createElement("thead"); // Table header.
    var tr = document.createElement("tr"); // Table header.

    for (var i = 0; i < col.length; i++) {
        var th = document.createElement("th"); // Table header.
        th.innerHTML = col[i];
        tr.appendChild(th);
    }
    thead.appendChild(tr);
    table.appendChild(thead);

    tr = table.insertRow(-1); // add new row for the names
    // Add JSON to the table rows.
    for (var i = 0; i < arrBirds.length; i++) {
        var tabCell = tr.insertCell(-1);
        tabCell.innerHTML = arrBirds[i].Name;
    }
    // Finally, add the dynamic table to a container.
    var divContainer = document.getElementById("showTable");
    divContainer.innerHTML = "";
    divContainer.appendChild(table);
};
