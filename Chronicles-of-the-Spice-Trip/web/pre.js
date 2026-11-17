// Runs before the game (linked in with --pre-js).
//
// The saves live in /save, which the browser keeps in IndexedDB between visits: it's read in
// before main starts, and Save.cpp calls Module.spicePersist() (through platform::persist)
// after every write.
//
// main also waits for the player's tap (Module.spiceStart, from web/shell.html), so that it
// starts inside the tap and the browser lets the game play sound. Module.spiceReady() tells
// the page when everything else is loaded.
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
    addRunDependency('spice-tap');
    Module['spiceStart'] = function () { removeRunDependency('spice-tap'); };

    FS.mkdir('/save');
    FS.mount(IDBFS, {}, '/save');
    addRunDependency('spice-saves');
    FS.syncfs(true, function (err) {
        if (err) console.warn('Spice Trip: could not read the saves', err);
        removeRunDependency('spice-saves');
        if (Module['spiceReady']) Module['spiceReady']();
    });
});

var spiceSaving = false, spiceSaveAgain = false;
Module['spicePersist'] = function () {
    if (spiceSaving) { spiceSaveAgain = true; return; }   // one copy at a time; the latest wins
    spiceSaving = true;
    FS.syncfs(false, function (err) {
        spiceSaving = false;
        if (err) console.warn('Spice Trip: could not keep the save', err);
        if (spiceSaveAgain) { spiceSaveAgain = false; Module['spicePersist'](); }
    });
};
