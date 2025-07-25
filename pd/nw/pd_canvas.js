"use strict";

var gui = require("nw.gui");
var pdgui = require("./pdgui.js");
var pd_menus = require("./pd_menus.js");

// Apply gui preset to this canvas
pdgui.skin.apply(window);

//var name = pdgui.last_loaded();

var l = pdgui.get_local_string;

function nw_window_focus_callback(name) {
    pdgui.set_focused_patchwin(name);
    pdgui.pdsend(name, "_focus", 1);
    // on OSX, update the menu on focus
    if (process.platform === "darwin") {
        nw_create_patch_window_menus(gui, canvas_events.get_id());
    }
}

function nw_window_blur_callback(name) {
    // Fake a mouseup event to keep from getting a dangling selection box
    pdgui.pdsend(name, "_focus", 0);
    if (canvas_events.get_state() === "normal") {
        pdgui.pdsend(name, "mouseup_fake");
    }
}

function nw_window_zoom(name, delta) {
    var z = gui.Window.get().zoomLevel;
    z += delta;
    if (z < 8 && z > -8) {
        //pdgui.post("nw_window_zoom");
        gui.Window.get().zoomLevel = z;
        pdgui.pdsend(name, "zoom", z);
        // ico@vt.edu 2022-12-05: super ugly since we have
        // no way of knowing when the zoom call has finished,
        // so that we can get accurate window dimensions
        // apparently, 0 does the trick
        // TODO!: need to test Win/OSX)
        setTimeout(function () { update_k12_menu(); }, 0);
        pdgui.gui_canvas_get_scroll(name);
    }
}

var canvas_events = (function() {
    var name,
        state,
        scalar_draggables = {}, // scalar child with the "drag" event enabled
        draggable_elem,         // last scalar we dragged
        draggable_label,        // kluge for handling [cnv] label/size anchors
        last_draggable_x,       // last x coord for the element we're dragging
        last_draggable_y,       // last y
        previous_state = "none", /* last state, excluding explicit 'none' */
        match_words_state = false,
        last_dropdown_menu_x,
        last_dropdown_menu_y,
        last_search_term = "",
        svg_view = document.getElementById("patchsvg").viewBox.baseVal,
        last_results = [], // last completion results (autocomplete)
        last_completed = -1, // last Tab completion (autocomplete)
        last_offset = -1, // offset of last Tab completion (autocomplete)
        last_yanked = "", // last yanked completion (to confirm deletion)
        pinch = 0, // used for monitor for pinch action, so that we don't
                   // erroneously call preventDefault when letting go and
                   // for a moment having one finger (if both are not let
                   // go at the same time)
        dropdown_mousedown = 0, // keeps track for touch interface if the
                                // mousedown already took place and avoids
                                // unnecessary redraw of the menu
        textbox = function () {
            return document.getElementById("new_object_textentry");
        },
        caret_end = function () {
            /* ag: Move the caret to the end of the texbox while editing. This
               is needed for the autcompletion. We essentially fake pressing
               the End key here; maybe there's an easier way to do this, but
               the following seems to work alright, so... We first grab the
               textbox content and determine its length, which is where we
               want the caret to be. */
            var t = textbox();
            var x = t.innerText;
            var p = x.length;
            //console.log("move "+p+": "+x);
            /* The DOM doesn't make this easy. We first have to define a
               range, set its start to the desired caret position, and
               collapse it to a single position (i.e., end = start). Next we
               grab the current selection, remove all currently selected
               ranges, and set our new range. Quite a hullaballoo for such a
               simple task. */
            var r = document.createRange();
            var s = window.getSelection();
            r.setStart(t.childNodes[0], p);
            r.collapse(true);
            s.removeAllRanges();
            s.addRange(r);
            // Defer this to the event loop to prevent losing the
            // keyboard focus.
            setTimeout(function () { t.focus() }, 0);
        },
        find_scalar_draggable = function (elem) {
            var ret = elem;
            while (ret) {
                if (scalar_draggables[ret.id]) {
                    return ret;
                }
                ret = (ret.parentNode && ret.parentNode.tagName === "g") ?
                    ret.parentNode : null;
            }
            return ret; // returning null if no parent group has a listener
        },
        target_is_scrollbar = function(evt) {
            // Don't send the event to Pd if we click on the scrollbars.
            // This is a bit fragile because we're suppressing on
            // HTMLHtmlElement which is too broad...
            if (evt.target.constructor.name === "HTMLHtmlElement") {
                return 1;
            } else {
                return 0;
            }
        },
        target_is_popup = function(evt) {
            // ag: Check whether the event actually goes to the confirmation
            // popup, in order to prevent special processing in touch events.
            // Like target_is_scrollbar above, this just looks for certain
            // constructor names, which may be fragile.
            if (evt.target.constructor.name == "HTMLButtonElement" ||
                evt.target.constructor.name == "HTMLSpanElement" ||
                evt.target.constructor.name == "HTMLDialogElement") {
                return 1;
            } else {
                return 0;
            }
        },
        target_is_canvas = function(evt) {
            // ag: Here we're looking for canvas touches, so that we can keep
            // scrolling enabled in this case. This check is a bit more
            // specific as we're also testing the parent class, so this is
            // hopefully a bit more robust in case of DOM changes.
            if (evt.target.constructor.name == "SVGSVGElement" &&
                evt.target.parentNode.classList.contains("patch_body")) {
                return 1;
            } else {
                return 0;
            }
        },
        target_is_canvasobj = function(evt) {
            function is_canvas_obj(target) {
                /*
                pdgui.post("1:" + target.classList.contains("obj") +
                           " 2:" + target.classList.contains("canvasobj") +
                           " 3:" + !target.classList.contains("graph") +
                           " classlist:" + target.classList);
                */
                return target.classList.contains("obj") &&
                    target.classList.contains("canvasobj") &&
                    !target.classList.contains("graph");
            };
            // ag: A bit of (maybe over-)defensive programming here: depending
            // on where exactly the user clicked, the actual object may be the
            // parent or the grandparent of the clicked target.
            if (evt.target.classList.contains("border") ||
                evt.target.classList.contains("box_text"))
                return is_canvas_obj(evt.target.parentNode);
            else if (evt.target.parentNode.classList.contains("box_text"))
                return is_canvas_obj(evt.target.parentNode.parentNode);
            else
                return is_canvas_obj(evt.target);
        },
        text_to_normalized_svg_path = function(text) {
            text = text.slice(4).trim()  // draw
                       .slice(4).trim()  // path
                       .slice(1).trim()  // d
                       .slice(1).trim(); // =
            if (text.slice(0, 1) === '"') {
                text = text.slice(1);
            }
            if (text.slice(-1) === '"') {
                text = text.slice(0, -1);
            }
            text = pdgui.parse_svg_path(text);
            return "draw path " + text.reduce(function (prev, curr) {
                return prev.concat(curr)
            }).join(" ");
        },
        text_to_fudi = function(text, iscomment) {
            text = text.trim();
            // special case for draw path d="arbitrary path string" ...
            if (text.search(/^draw\s+path\s+d\s*=\s*"/) !== -1) {
                text = text_to_normalized_svg_path(text);
            }
            //pdgui.post("text_to_fudi:\n...before <"+text+">");
            // escape dollar signs
            //text = text.replace(/(\\\$[0-9]+)/g, "\\\$1");
            text = text.replace(/(\$[0-9]+)/g, "\\$1");

            // escape special $@ sign
            //text = text.replace(/(\\\$@)/g, "\\\$@");
            text = text.replace(/(\$@)/g, "\\$@");

            
            //var j;
            //for(j=0;j<text.length;j++)
            //    pdgui.post("..."+text[j].charCodeAt()+"("+text[j]+")");

            // filter consecutive ascii32 OR deal with spaces and
            // \n symbols inside comments to retain them. Also,
            // have to carefully treat \n by themselves (whcih become
            // \v). We add a space to each of them to ensure that
            // added lines are properly displayed in the comment.
            if (iscomment === 1) {
                // 2021-05-21: we need the following to get rid of
                // extra endlines that were created after semis by
                // the c code, so that we can match the original state
                // LATER: consider adding endlines only inside js
                // UPDATE: later has already happened, so this is not
                // necessary anymore
                //pdgui.post("comment_to_fudi before <" + text + ">");
                //text = text.replace(/;\n/g, ";");
                //pdgui.post("................after <" + text + ">");

                // escape "," and ";"
                //text = text.replace(/(?<!\\)(;|,)/g, "\\$1");
                text = text.replace(/(\\;|\\,)/g, "\\\\$1");
                text = text.replace(/(?<!\\)(;|,)/g, "\\$1");
                text = text.replace(/(?<!\ |\\)(\\;|\\,)/g, " $1");
                text = text.replace(/(\\;|\\,)(?!\ )/g, "$1 ");
                text = text.replace(/(\\ )/g, " ");
                text = text.replace(/\u0020+/g, " ");

                // substitute \n, so that we can preserve
                // comment formatting
                text = text.replace(/\n/g, "\v");
                //text = text.replace(/ /g, "\u00A0");
                var lines = text.split('\v');
                var i;//, j;
                text = "";
                for(i = 0; i < lines.length; i++) {
                    //if (lines[i] == "")
                    //    lines[i] = "\u00A0";
                    if (i < lines.length - 1) {
                        text = text.concat(lines[i], '\v');
                    } else {
                        text = text.concat(lines[i]);
                    }
                    /*
                    pdgui.post(">>>>>>>>>>>>>>");
                    for(j=0;j<lines[i].length;j++)
                        pdgui.post("..."+lines[i][j].charCodeAt());
                    */
                }
                /*
                pdgui.post("================================");
                for(j=0;j<text.length;j++)
                    pdgui.post("..."+text[j].charCodeAt());
                */
            } else {
                // escape , and ; and differentiate between those
                // that are real and those that are just written (pre-escaped)
                // NOTE: if the escaped , and ; is not preceded by a space
                // it does not need triple \\\ before it since it will be 
                // bundled with the symbol starting with letters before it.
                // if it is standing by itself, it will be escaped by \\\ later
                // on the c side of things, as reflected in the saved file.
                // therefore abc\, will be escaped in a way that treats it
                // simply as a character, while abc \, will result in a functional
                // comma, becoming abc \\\, when saved.
                text = text.replace(/(\\;|\\,)/g, "\\\\$1");
                text = text.replace(/(?<!\\)(;|,)/g, "\\$1");
                text = text.replace(/(?<!\ |\\)(\\;|\\,)/g, " $1");
                text = text.replace(/(\\;|\\,)(?!\ )/g, "$1 ");
                //text = text.replace(/(\\ )/g, " ");

                // get rid of extra consecutive spaces, including escaped ones
                // until we figure out a better way to handle those
                text = text.replace(/(\\ )/g, " ");
                text = text.replace(/\u0020+/g, " ");
            }
            // we need to make sure that the very last character is not a backslash
            // which can escape the final semicolon and cause a lot of problems...
            //pdgui.post("...last char=<" + text[text.length-1] +">");
            if (text[text.length-1] === '\\') {
                //pdgui.post("...got a backslash as the last char...");
                text = text.replace(/.$/,'\0');
            }
            //pdgui.post("...after  <"+text+">");
            return text;
        },
        string_to_array_of_chunks = function(msg) {
        // Convert a string (FUDI message) to an array of strings small enough
        // to fit in Pd's socketreceiver buffer of 4096 bytes
            var chunk_max = 1024,
                max_pd_string = 1000,
                left,
                in_array = [],
                nargs = [],
                out_array = [];
            if (msg.length <= chunk_max) {
                out_array.push([msg]);
            } else {
                /*
                in_array = msg.split(/[\s\n]/); // split on newlines or spaces
                while (in_array.length) {
                    left = in_array.slice(); // make a copy of in_array
                    if (left.toString().length > chunk_max) {
                        while (1) {
                            if (left.length < 2) {
                                pdgui.post("Warning: string truncated:");
                                pdgui.post(left.toString().
                                    match(/............................../g).join("\n")
                                );
                                break;
                            }
                            left = left.splice(0, left.length >> 1);
                            if (left.toString().length <= chunk_max) {
                                break;
                            }
                        }
                    }

                    // might need a check here for max_pd_string to warn
                    // user if a string is going to get truncated. (That's
                    // what max_pd_string is for above.)
                    out_array.push(left);
                    in_array = in_array.splice(left.length);
                }
                */
                // ico@vt.edu 2021-11-13: replacing buggy implementation
                // above that caused problems when a comment had more than
                // 1024 characters, which resulted in endlines being removed
                // TODO: Still need to warn the user if the number of
                // arguments exceeds that of pd's 1000.
                nargs = msg.split(' ');
                //pdgui.post("nargs="+nargs.length);
                if (nargs.length > max_pd_string)
                    pdgui.post("warning: message exceeds pd-l2ork's " +
                         "allowed maximum of 1000 arguments. excess " +
                         "arguments will be truncated...");
                in_array = msg.match(/.{1,50}(\ |$)/g);
                in_array.forEach(function(entry) {
                    //pdgui.post(entry);
                    if (entry !== "")
                        out_array.push(entry);
                });
            }
            /*
            var i;
            for (i = 0; i < out_array.length; i++)
                pdgui.post("..."+out_array[i]+"\n================\n");
            */
            return out_array;
        },
        might_be_a_pd_file = function(stuff_from_clipboard) {
            // Super-simplistic guess at whether the string from the clipboard
            // starts with Pd code. This is just meant as a convenience so that
            // stuff in the copy buffer that obviously isn't Pd code doesn't get
            // in the way when editing.
            var text = stuff_from_clipboard.trim(),
                one = text.charAt(0),
                two = text.charAt(1);
            return (one === "#" && (two === "N" || two === "X"));
        },
        grow_svg_for_element = function(elem) {
            // See if an element overflows the svg bbox, and
            // enlarge the svg to accommodate it.
            // Note: window.scrollX and window.scrollY might not work
            // with Android Chrome 43 and IE.
            var svg = document.getElementById("patchsvg"),
                elem_bbox = elem.getBoundingClientRect(),
                svg_viewbox = svg.getAttribute("viewBox").split(" "),
                w = Math.max(elem_bbox.left + elem_bbox.width + window.scrollX,
                    svg_viewbox[2]),
                h = Math.max(elem_bbox.top + elem_bbox.height + window.scrollY,
                    svg_viewbox[3]);
            svg.setAttribute("viewBox",
                [Math.min(elem_bbox.left, svg_viewbox[0]),
                 Math.min(elem_bbox.top, svg_viewbox[1]),
                 w,
                 h
                ].join(" "));
            svg.setAttribute("width", w);
            svg.setAttribute("height", h);
        },
        dropdown_index_to_pd = function(elem) {
            var highlighted = elem.querySelector(".highlighted");
            //pdgui.post("highlighted=" + highlighted);
            if (highlighted) {
                pdgui.pdsend(elem.getAttribute("data-callback"),
                    highlighted.getAttribute("data-index"));
                return true;
            }
            return false;
        },
        dropdown_clear_highlight = function() {
            var container = document.querySelector("#dropdown_list"),
                li_array = container.querySelectorAll("li");
            Array.prototype.forEach.call(li_array, function(e) {
                e.classList.remove("highlighted");
            });
        },
        dropdown_highlight_elem = function(elem, scroll) {
            var container = document.querySelector("#dropdown_list");
            if (!elem.classList.contains("highlighted")) {
                dropdown_clear_highlight();
                elem.classList.add("highlighted");
                // Make sure the highlighted element is in view
                if (scroll) {
                    if (elem.offsetTop < container.scrollTop
                        || elem.offsetTop + elem.offsetHeight >
                            container.scrollTop + container.offsetHeight) {
                            if (scroll === "up") {
                                // we can't usse elem.scrollIntoView() here
                                // because it may also change the scrollbar on
                                // the document, which in turn could change the
                                // pageX/pageY, for the mousemove event, which
                                // in turn would make it impossible for us to
                                // filter out unnecessary mousemove calls
                                //    elem.scrollIntoView();
                            container.scrollTop = elem.offsetTop;
                        } else if (scroll === "down") {
                            container.scrollTop = elem.offsetTop + elem.offsetHeight
                                - container.clientHeight;
                        }
                    }
                }
            }
        },
        ac_dropdown = function() {
            return document.getElementById("autocomplete_dropdown")
        },
        // AG: Little helper function to do all the necessary steps to
        // re-create the autocompletion dropdown after changes. We factored
        // this out since it will be needed to deal with changes to both the
        // edited object text and the autocompletion index.
        ac_repopulate = function() {
            // GB: Finding the class from obj: find obj through tag of
            // textbox, get obj class and remove from the class the word
            // "selected". This is necessary because in textbox obj and
            // comment both have class 'obj' and it's import here to
            // differentiate them.
            let obj_class = document
                .getElementById(textbox().getAttribute("tag")+"gobj")
                .getAttribute("class").toString()
                .split(" ").slice(0,1).toString();
            //pdgui.post("ac_repopulate... class=" + obj_class);
            if (obj_class === "obj") { // autocomplete only works for objects
                //pdgui.post("...got object");
                pdgui.create_autocomplete_dd(name, document, ac_dropdown(), textbox());
                /*
                pdgui.post("...searched_text=<" +
                    ac_dropdown().getAttribute("searched_text") +
                    "> innerText=<" + textbox().innerText + ">");
                */
                // ico@vt.edu 2022-11-09: the following if statement prevents
                // repopulating values if the innerText is the same as the one
                // being removed
                //if (ac_dropdown().getAttribute("searched_text") !== textbox().innerText) {
                //pdgui.post("ac_repopulate calling repopulate_autocomplete_dd");
                last_results = pdgui.repopulate_autocomplete_dd(name, document, ac_dropdown, obj_class, textbox());
                last_offset = 0;
                //}
            }
        },
        events = {
            mousemove: function(evt) {
                //pdgui.post("mousemove");
                //pdgui.post("x: " + evt.pageX + " y: " + evt.pageY +
                //    " modifier: " + (evt.shiftKey + (pdgui.cmd_or_ctrl_key(evt) << 1)));
                if (evt.type === "touchmove") {
                    //pdgui.post("is touch " +  evt.touches.length);
                    if (target_is_popup(evt)) {
                        // ag: Presumably this is the confirmation popup, we
                        // don't want to do any special processing there at
                        // all, so bail out immediately and let the default
                        // handlers take over.
                        return;
                    } else if (m.edit.editmode.checked &&
                               !pdgui.cmd_or_ctrl_key(evt)) {
                        // ag: Disable touch scrolling while we're doing a
                        // selection-drag, for the same reasons we're
                        // suppressing it while touching an object (see
                        // mousedown, below). Note that to scroll in edit mode
                        // you can just press the Alt (used to be Ctrl/Cmd) key
                        // while dragging, which will also suppress the selection
                        // rectangle to appear.

                        // ico@vt.edu 2023-10-22: here we once again distinguish
                        // between single touch (which should disable scrolling)
                        // and double touch (a.k.a. pinch) and adjust accordingly
                        if (evt.touches.length === 1) {
                            //pdgui.post("suppress scroll");
                            //document.body.style.overflow = 'hidden';
                            //document.body.style.touchAction = 'none';
                            //document.getElementById("patchsvg").
                            //    style.touchAction = 'none';
                            if (pinch !== 1) {
                                if (evt.cancelable) evt.preventDefault();
                            }
                        } else {
                            // we are pinching in edit mode
                            //pdgui.post("editmode pinch");
                            //document.body.style.overflow = 'visible';
                            //document.body.style.touchAction = 'auto';
                            //document.getElementById("patchsvg").
                            //    style.touchAction = 'auto';
                            return false;
                        }
                    } else {
                        //document.body.style.touchAction = 'auto';
                        //document.getElementById("patchsvg").
                        //        style.touchAction = 'auto';
                        //pdgui.post("runtime");
                        if (evt.touches.length === 1) {
                            //document.body.style.overflow = 'hidden';
                            //document.body.style.touchAction = 'none';
                            //document.getElementById("patchsvg").
                            //    style.touchAction = 'none';
                            if (pinch !== 1) {
                                if (evt.cancelable) evt.preventDefault();
                            }
                        } else {
                            // we are pinching in runtime mode
                            return false;
                        }
                    }
                }
                // ag: It seems possible to get fractional values here, which
                // pdsend doesn't seem to like, so we truncate the touch
                // coordinates to integers.
                let [pointer_x, pointer_y] = evt.type === "touchmove"
                    ? [Math.trunc(evt.touches[0].pageX),
                       Math.trunc(evt.touches[0].pageY)]
                    : [evt.pageX, evt.pageY];
                pdgui.pdsend(name, "motion",
                    (pointer_x + svg_view.x),
                    (pointer_y + svg_view.y),
                    (evt.shiftKey + (pdgui.cmd_or_ctrl_key(evt) << 1))
                );
                // Commented lines below due to interference with touch scroll behavior
                //evt.stopPropagation();
                //evt.preventDefault();
                return false;
            },
            activatedmousemove: function(evt) {
                let [pointer_x, pointer_y] = evt.type === "touchmove"
                    ? [Math.trunc(evt.touches[0].pageX),
                       Math.trunc(evt.touches[0].pageY)]
                    : [evt.pageX, evt.pageY];
                pdgui.pdsend(name, "text_activated_motion",
                    (pointer_x + svg_view.x),
                    (pointer_y + svg_view.y),
                    (evt.shiftKey + (pdgui.cmd_or_ctrl_key(evt) << 1))
                );
            },
            mousedown: function(evt) {
                //pdgui.post("mousedown");
                //pdgui.post("pdcanvas mousedown evt.Altkey=" +
                //    evt.altKey + " tgt_is_canvas=" + target_is_canvas(evt));
                // ico@vt.edu capture middle click for a different type of scroll
                // currently disabled due to problem with scrollBy and zoom
                /*if (evt.which == 2)
                {
                    evt.stopPropagation();
                    evt.preventDefault();                
                }*/
                if (evt.type === "touchstart") {
                    //pdgui.post("touchstart " + evt.touches.length);
                    if (target_is_popup(evt)) {
                        return;
                    } else if (!target_is_canvas(evt)) {
                        // ag: Disable touch scrolling while we're operating a
                        // GUI element, since trying to operate a slider while
                        // the canvas keeps moving beneath you is difficult.
                        // Actually, we're not overly specific here, we just
                        // test whether we're touching anything but the canvas
                        // itself. So, to initiate a scroll using touch,
                        // you'll have to touch a blank spot on the canvas.

                        // ico@vt.edu 2023-10-22: this is problematic because
                        // some interfaces (like L2Ork Tweeter) are densely
                        // packed, leaving no place to scroll over, so
                        // we only disable this if there is only one touch
                        // present, whereas two touches suggest
                        // pinch/zoom/scroll event.
                        //pdgui.post("num touches=" + evt.touches.length);
                        if (evt.touches.length === 1) {
                            //document.body.style.overflow = 'hidden';
                            //document.body.style.touchAction = 'none';
                            //document.getElementById("patchsvg").
                            //    style.touchAction = 'none';
                            // this works for everything except when you
                            // are trying to move the canvas while
                            // hovering over objects, like in Tweeter
                            /*if (pinch !== 1) {
                                setTimeout(function() {
                                    pdgui.post("delay");
                                    if (evt.cancelable && pinch !== 1) {
                                        evt.preventDefault();
                                        pdgui.post("yes");
                                    }
                                }, 10);
                                //if (evt.cancelable) evt.preventDefault();
                            }*/
                            //pdgui.post("...one touch");
                        } else {
                            //pdgui.post("...pinch");
                            //document.body.style.overflow = 'visible';
                            //document.body.style.touchAction = 'auto';
                            //document.getElementById("patchsvg").
                            //    style.touchAction = 'auto';
                            pinch = 1;
                            return false;
                        }
                    } else {
                        //document.body.style.touchAction = 'auto';
                        //document.getElementById("patchsvg").
                        //    style.touchAction = 'auto';
                    }
                }
                let [pointer_x, pointer_y] = evt.type === "touchstart"
                    ? [Math.trunc(evt.touches[0].pageX),
                       Math.trunc(evt.touches[0].pageY)]
                    : [evt.pageX, evt.pageY];
                last_dropdown_menu_x = pointer_x;
                last_dropdown_menu_y = pointer_y;
                var target_id, resize_type;
                if (target_is_scrollbar(evt)) {
                    return;
                } else if (evt.target.parentNode &&
                    evt.target.parentNode.classList
                        .contains("clickable_resize_handle")) {
                    draggable_label =
                        evt.target.parentNode.classList.contains("move_handle");

                    // get id ("x123456etcgobj" without the "x" or "gobj")
                    target_id = (draggable_label ? "_l" : "_s") +
                        evt.target.parentNode.parentNode.id.slice(0,-4).slice(1);
                    last_draggable_x = pointer_x + svg_view.x;
                    last_draggable_y = pointer_y + svg_view.y;

                    // Nasty-- we have to forward magic values from g_canvas.h
                    // defines in order to get the correct constrain behavior.
                    if (evt.target.classList.contains("constrain_top_right")) {
                        resize_type = 7; // CURSOR_EDITMODE_RESIZE_X
                    } else if (evt.target.classList
                        .contains("constrain_bottom_right")) {
                        resize_type = 10; // CURSOR_EDITMODE_RESIZE_Y
                    } else if (draggable_label) {
                        resize_type = 11; // CURSOR_EDITMODE_MOVE
                    } else {
                        resize_type = 8; // CURSOR_EDITMODE_RESIZE
                    }

                    // Even nastier-- we now must turn off the cursor styles
                    // so that moving the pointer outside the hotspot doesn't
                    // cause the cursor to change. This happens for the
                    // drag handle to move the gop red rectangle. Unlike
                    // the label handles, it doesn't get immediately
                    // destroyed upon receiving its callback below.
                    pdgui.toggle_drag_handle_cursors(evt.target.parentNode,
                        !!draggable_label, false);

                    // make all label motion touches movable in both directions
                    if (evt.type == "touchstart" &&
                            (resize_type === 7 || resize_type === 10)) {
                        resize_type = 11;
                    }

                    //pdgui.post("_click resize_type=" + resize_type);
                    pdgui.pdsend(target_id, "_click", resize_type,
                        (pointer_x + svg_view.x),
                        (pointer_y + svg_view.y));
                    canvas_events.iemgui_label_drag();
                    evt.preventDefault();
                    return;
                }
                // tk events (and, therefore, Pd events) are one greater
                // than html5...
                var b = evt.button + 1 || 1;
                var mod, match_elem;
                // See if there are any draggable scalar shapes...
                if (Object.keys(scalar_draggables).length) {
                    // if so, see if our target is one of them...
                    match_elem = find_scalar_draggable(evt.target);
                    if (match_elem) {
                        // then set some state and turn on the drag events
                        draggable_elem = match_elem;
                        last_draggable_x = pointer_x;
                        last_draggable_y = pointer_y;
                        canvas_events.scalar_drag();
                    }
                }
                // For some reason right-click sends a modifier value of "8",
                // and canvas_doclick in g_editor.c depends on that value to
                // do the right thing.  So let's hack...
                if (b === 3 || (process.platform === "darwin" && evt.ctrlKey)) {
                    // right-click
                    mod = 8;
                } else {
                    mod = (evt.shiftKey + (pdgui.cmd_or_ctrl_key(evt) << 1));
                }
                //pdgui.post("mousedown " + (pointer_x + svg_view.x) + "," +
                //    (pointer_y + svg_view.y) + " " + b + " " + mod);
                if (evt.type === "touchstart" && evt.touches.length === 2) {
                    // if we have just added a second touch, meaning we are
                    // wanting to pinch, we negate the first touchstart
                    // to prevent doing an invisible selection at touchend
                    //pdgui.post("override" + (pointer_x + svg_view.x) + "," +
                    //(pointer_y + svg_view.y) + " " + ((evt.button + 1) || 1));
                    pdgui.pdsend(name, "mouseup",
                        (pointer_x + svg_view.x),
                        (pointer_y + svg_view.y),
                        (evt.button + 1) || 1
                    );
                } else {
                    //pdgui.post("mousedown");
                    pdgui.pdsend(name, "mouse",
                        (pointer_x + svg_view.x),
                        (pointer_y + svg_view.y),
                        b, mod
                    );
                }
                // If Alt is pressed on a box_text, fake a keyup to prevent
                // dangling temp runmode in case the click opens a subpatch.
                if (evt.altKey && target_is_canvasobj(evt)) {
                    //pdgui.post("generating fake mouse up...");
                    pdgui.canvas_sendkey(name, 0, evt, "Alt", 0);
                }
                //evt.stopPropagation();
                //evt.preventDefault();
            },
            mouseup: function(evt) {
                //pdgui.post("mouseup");
                //pdgui.post("mouseup: x: " +
                //    evt.pageX + " y: " + evt.pageY +
                //    " button: " + (evt.button + 1));
                if (evt.type === "touchend") {
                    //pdgui.post("touchend " + Math.trunc(evt.changedTouches[0].pageX) +
                    //    "," + Math.trunc(evt.changedTouches[0].pageY) +
                    //    " touches=" + evt.touches.length);
                    if (target_is_popup(evt)) {
                        return;
                    }
                    // re-enable scrolling
                    document.body.style.overflow = 'visible';
                    //pdgui.post("scale=" + window.visualViewport.scale);
                    // ico@vt.edu 2023-10-22: only re-enable touch-based
                    // scrolling if we are on pinch scale of 1. otherwise,
                    // when trying to manipulate objects while scaled in
                    // (this is different from the menu zoom options),
                    // this would also cause canvas to move with finger
                    // movement, making it impossible to manipulate objects
                    // or their values
                    //if (window.visualViewport.scale === 1) {
                    // ONLY NEXT THREE LINES
                    //    document.body.style.touchAction = 'auto';
                    //    document.getElementById("patchsvg").
                    //        style.touchAction = 'auto';
                    //}
                }
                let [pointer_x, pointer_y] = evt.type === "touchend"
                    ? [Math.trunc(evt.changedTouches[0].pageX),
                       Math.trunc(evt.changedTouches[0].pageY)]
                    : [evt.pageX, evt.pageY];
                //pdgui.post("mouseup " + (pointer_x + svg_view.x) + "," +
                //    (pointer_y + svg_view.y) + " " + ((evt.button + 1) || 1));
                pdgui.pdsend(name, "mouseup",
                    (pointer_x + svg_view.x),
                    (pointer_y + svg_view.y),
                    (evt.button + 1) || 1
                );
                evt.stopPropagation();
                // this causes errors when letting go of double touch
                // (a.k.a. pinch)
                if (evt.cancelable) evt.preventDefault();
                if (evt.type === "touchend" && evt.touches.length === 0) {
                    pinch = 0;
                }
            },
            keydown: function(evt) {
                pdgui.keydown(name, evt);
                // prevent the default behavior of scrolling using arrow keys
                if (document.querySelector("#patchsvg")) {
                    if ([37, 38, 39, 40].indexOf(evt.keyCode) > -1) {
                        evt.preventDefault();
                    }
                }
            },
            keypress: function(evt) {
                pdgui.keypress(name, evt);
                // Don't do things like scrolling on space, arrow keys, etc.
                evt.preventDefault();
            },
            keyup: function(evt) {
                pdgui.keyup(name, evt);
                evt.stopPropagation();
                evt.preventDefault();
            },
            text_mousemove: function(evt) {
                if (evt.target.parentNode === document.getElementById("autocomplete_dropdown")) {
                    let sel = document.getElementById("autocomplete_dropdown").getAttribute("selected_item");
                    let new_sel = evt.target.getAttribute("idx");
                    pdgui.update_autocomplete_selected(document.getElementById("autocomplete_dropdown"), sel, new_sel);
                }
                //pdgui.post("text_mousemove");
                events.activatedmousemove(evt);
                evt.stopPropagation();
                //evt.preventDefault();
                return false;
            },
            text_mousedown: function(evt) {
                if (evt.target.parentNode === ac_dropdown()) {
                    pdgui.select_result_autocomplete_dd(textbox(), ac_dropdown(), last_completed, last_offset, last_results, evt.shiftKey?-1:1);
                    last_yanked = "";
                    // ag: Don't do the usual object instantiation thing if
                    // we've clicked on the autocompletion dropdown. This
                    // means that the user can just go on editing, entering
                    // object arguments, etc.
                    evt.stopPropagation();
                    //evt.preventDefault();
                    caret_end();
                    return false;
                }
                // here we check whether we are also not a child of a parentnode.
                // this happens when one presses return while editing the object
                // text via new_object_textentry (see pdgui.js), which causes
                // the text to be split between the main <p> and a child <div>
                if (!textbox().contains(evt.target) && !target_is_scrollbar(evt)) {
                    utils.create_obj();
                    // send a mousedown and mouseup event to Pd to instantiate
                    // the object
                    events.mousedown(evt);
                    //events.mouseup(evt);
                    canvas_events.normal();
                }
                evt.stopPropagation();
                //evt.preventDefault();
                return false;
            },
            text_mouseup: function(evt) {
                //pdgui.post("mouseup target is " +
                //    evt.target + " and textbox is " + textbox());
                //evt.stopPropagation();
                //evt.preventDefault();
                return false;
            },
            text_keydown: function(evt) {
                //pdgui.post("keydown");
                evt.stopPropagation();
                setTimeout(function() {
                    pdgui.gui_message_update_textarea_border(name, textbox(), 1);
                }, 0);

                // ico@vt.edu 2022-10-13: moved autocomplete to keydown to allow
                // for autorepeat
                let ac_dropdown = function() {
                    return document.getElementById("autocomplete_dropdown");
                }
                switch (evt.keyCode) {
                    case 40: // arrowdown
                        pdgui.update_autocomplete_dd_arrowdown(ac_dropdown())
                        last_yanked = "";
                        break;
                    case 38: // arrowup
                        pdgui.update_autocomplete_dd_arrowup(ac_dropdown())
                        last_yanked = "";
                        break;
                }            
                //evt.preventDefault();
                return false;
            },
            text_keyup: function(evt) {
                evt.stopPropagation();

                // GB: Autocomplete feature
                let ac_dropdown = function() {
                    return document.getElementById("autocomplete_dropdown");
                }
                switch (evt.keyCode) {
                    case 13: // enter
                        // if there is no item selected on autocomplete dropdown, enter make the obj box bigger
                        if(ac_dropdown() === null || ac_dropdown().getAttribute("selected_item") === "-1") {
                            grow_svg_for_element(textbox());
                        } else { // else, if there is a selected item on autocompletion tool, the selected item is written on the box
                            pdgui.select_result_autocomplete_dd(textbox(), ac_dropdown(), last_completed, last_offset, last_results, evt.shiftKey?-1:1);
                            caret_end();
                            // No need to instantiate the object here,
                            // presumably the user wants to go on editing.
                        }
                        last_yanked = "";
                        break;
                    case 9: // tab
                        [last_completed, last_offset] = pdgui.select_result_autocomplete_dd(textbox(), ac_dropdown(), last_completed, last_offset, last_results, evt.shiftKey?-1:1);
                        last_yanked = "";
                        caret_end();
                        break;
                    case 36:
                        if (evt.altKey) { // alt-home
                            [last_completed, last_offset] = pdgui.select_result_autocomplete_dd(textbox(), ac_dropdown(), 0, last_offset, last_results, 0);
                            last_yanked = "";
                            caret_end();
                        }
                        break;
                    case 35:
                        if (evt.altKey) { // alt-end
                            [last_completed, last_offset] = pdgui.select_result_autocomplete_dd(textbox(), ac_dropdown(), last_results.length-1, last_offset, last_results, 0);
                            last_yanked = "";
                            caret_end();
                        }
                        break;
                    case 27: // esc
                        pdgui.delete_autocomplete_dd(ac_dropdown());
                        last_completed = last_offset = -1;
                        last_results = [];
                        if (last_yanked != "") {
                            pdgui.post("Operation aborted.")
                        }
                        last_yanked = "";
                        break;
                    case 89:
                        if (evt.ctrlKey) { // ctrl-y
                            let sel = ac_dropdown().getAttribute("selected_item");
                            var text_to_remove;
                            if (sel > -1) {
                                text_to_remove = ac_dropdown().children.item(sel).innerText;
                            } else {
                                text_to_remove = textbox().innerText;
                            }
                            //pdgui.post("autocomplete ctrl-y text_to_remove=" + text_to_remove);
                            // AG: Note that this key is usually bound to the
                            // Tidy Up operation in the Edit menu, but this
                            // presumably won't interfere with our use here,
                            // which is to "yank" the current completion
                            // (remove it from the completion index).
                            last_completed = last_offset = -1;
                            last_results = [];
                            if (textbox().innerText === "") {
                                pdgui.delete_autocomplete_dd(ac_dropdown());
                                last_yanked = "";
                            } else if (text_to_remove === last_yanked) {
                                // confirmed, really yank now
                                if (pdgui.remove_completion(text_to_remove, console.log)) {
                                    pdgui.post("Removed completion: " + text_to_remove);
                                    ac_repopulate();
                                    last_results = [];
                                    last_completed = last_offset = -1;
                                }
                                last_yanked = "";
                            } else if (pdgui.check_completion(text_to_remove)) {
                                // for some safety, ask the user to confirm with another ctrl+y
                                pdgui.post("Really remove completion " + text_to_remove + "?");
                                pdgui.post("Press ctrl+y again to confirm (Esc to abort).");
                                last_yanked = text_to_remove;
                            } else {
                                pdgui.post("No completion " + text_to_remove);
                            }
                        }
                        break;
                    default:
                        // ag: Only update the state if a "valid key" is
                        // pressed, not some modifier or other special key.
                        function is_valid_key(e)
                        {
                            // See https://stackoverflow.com/questions/51296562
                            // This is a quick hack which works because the
                            // names of special keys are all multiple chars
                            // long and only contain letters and numbers.
                            return e.key.length == 1 // ASCII
                                // non-ASCII:
                                || (e.key.length > 1 && /[^a-zA-Z0-9]/.test(e.key))
                                // Spacebar:
                                || e.keyCode == 32
                                // We also include Backspace and Del here
                                // since they are used in editing. Note that
                                // Return (key code 13) is handled above.
                                || e.keyCode == 8 || e.keyCode == 46;
                        }
                        if (is_valid_key(evt)) {
                            last_results = [];
                            last_completed = last_offset = -1;
                            last_yanked = "";
                            if (textbox().innerText === "") {
                                pdgui.delete_autocomplete_dd(ac_dropdown());
                            } else {
                                ac_repopulate();
                            }
                        }
                }

                //evt.preventDefault();
                return false;
            },
            text_keypress: function(evt) {
                evt.stopPropagation();
                //evt.preventDefault();
                return false;
            },
            text_paste: function(evt) {
                evt.preventDefault();
                document.execCommand("insertText", false,
                    evt.clipboardData.getData("text"));
                grow_svg_for_element(textbox());
            },
            floating_text_click: function(evt) {
                if (target_is_scrollbar(evt)) {
                    return;
                }
                //pdgui.post("leaving floating mode");
                // refocus the textarea because toggling the k12 menu
                // in the process somehow makes us lose this focus
                document.getElementById("new_object_textentry").focus();
                //pdgui.post("refocusing textentry...");
                /* here we revert the cursor for the textentry to default.
                   this was activated at the creation time, see
                   canvas_startmotion inside the g_editor.c. after we
                   have done this, we update the scrollbars in case
                   the element has been placed in a way that the scrollbars
                   become unnecessary. */
                textbox().style.setProperty("cursor", "initial");
                pdgui.gui_canvas_get_immediate_scroll(name);
                canvas_events.text();
                evt.stopPropagation();
                evt.preventDefault();
                return false;
            },
            floating_text_keypress: function(evt) {
                //pdgui.post("leaving floating mode");
                // refocus the textarea because toggling the k12 menu
                // in the process somehow makes us lose this focus
                document.getElementById("new_object_textentry").focus();
                //pdgui.post("refocusing textentry...");
                canvas_events.text();
                //evt.stopPropagation();
                //evt.preventDefault();
                //return false;
            },
            find_click: function(evt) {
                //pdgui.post("pdcanvas find_click");
                var t = document.getElementById("canvas_find_text").value;
                //var w = document.getElementById("canvas_find_whole_word").checked;
                //pdgui.post("whole="+(w ? "1" : "0"))
                /*
                pdgui.post("find_click " + t + " " +
                    canvas_events.get_last_search_term() + " " +
                    pdgui.gui_get_backend_search_term());
                */
                //pdgui.post("origin="+pdgui.gui_get_search_origin_canvas()+" name="+name);
                if (t !== "") {
                    if (pdgui.gui_get_search_origin_canvas() === name &&
                        t === canvas_events.get_last_search_term() &&
                        t === pdgui.gui_get_backend_search_term()) {
                        pdgui.pdsend(name, "findagain");
                    } else {
                        pdgui.pdsend(name, "find",
                        pdgui.encode_for_dialog(t),
                        match_words_state ? "1" : "0");
                        pdgui.gui_set_search_origin_canvas(name);
                    }
                    canvas_events.set_last_search_term(t);
                }
                // ico@vt.edu 2021-10-06:
                // we do not need these here because the find_bar has its
                // catch-all find_ignore event and is located below all
                // find widgets and above the actual canvas
                //evt.stopPropagation();
                //evt.preventDefault();
            },
            find_ignore: function(evt) {
                //pdgui.post("find_ignore");
                evt.stopPropagation();
                // ico@vt.edu 2021-10-06: if we also preventDefault, the
                // entry box cannot be focused anymore
                //evt.preventDefault();
            },
            find_keydown: function(evt) {
                if (evt.keyCode === 13) {
                    events.find_click(evt);
                }
            },
            match_words: function(evt) {
                // ico@vt.edu 2021-10-06: curiously we need to make the
                // variable equal to inverted value of the checkbox
                match_words_state =
                    !document.getElementById("canvas_find_whole_word").checked;
                //pdgui.post("whole_words " + match_words_state);
                // ico@vt.edu 2021-10-06: reset find process since we have
                // changed parameters, so findagain now makes no more sense
                canvas_events.find_reset();
                pdgui.gui_set_backend_search_term("");
                pdgui.gui_set_search_origin_canvas(name);
            },
            scalar_draggable_mousemove: function(evt) {
                let [new_x, new_y] = evt.type === "touchmove"
                    ? [Math.trunc(evt.touches[0].pageX),
                       Math.trunc(evt.touches[0].pageY)]
                    : [evt.pageX, evt.pageY],
                    dx = new_x - last_draggable_x,
                    dy = new_y - last_draggable_y,
                    // For the sake of convenience we're sending transformed
                    // dx and dy back to the user. If it turns out this is
                    // expensive we can either optimize this or just leave it
                    // up to the user to handle on their own. (But I should
                    // mention that it was non-trivial for me to do the math
                    // of inverting and multiplying the matrices from within
                    // a Pd patch. And I'm the author of this API. Make
                    // of that what you will...)
                    minv = draggable_elem.getCTM().inverse(),
                    tx = minv.a * dx + minv.c * dy,
                    ty = minv.b * dx + minv.d * dy;
                var obj = scalar_draggables[draggable_elem.id];
                pdgui.pdsend(obj.cid, "scalar_event", obj.scalar_sym,
                    obj.drawcommand_sym, obj.array_sym, obj.index,
                    obj.event_name, dx, dy, tx, ty);
                last_draggable_x = new_x;
                last_draggable_y = new_y;
            },
            scalar_draggable_mouseup: function(evt) {
                canvas_events.normal();
            },
            iemgui_label_mousemove: function(evt) {
                // This is very convoluted.
                // 1. In the generic mousedown handler we detect a click for a
                //    label handle, red gop rect handle, or [cnv] resize anchor.
                //    That sets this callback for dragging the handle.
                // 2. The mousedown also sends a message to Pd to tell it that
                //    a handle has been clicked. The message is forwarded
                //    to the relevant handle (a t_scalehandle in Pd).
                // 3. Pd erases *all* handles, then redraws the one for this
                //    object. That *eventually* leaves just the handle(s)
                //    for the current object. This is either a single handle
                //    (for most iemguis and the canvas red rect), or possibly
                //    two handles for [cnv]-- one for resizing and one for its
                //    label.
                // 4. This function responds to mouse motion. It looks up
                //    the current handle by tag (using the draggable_lable
                //    kludge to choose between the cnv_resize_handle and
                //    everything else), sends a message to Pd to alter the
                //    object/label accordingly, then displaces the little
                //    handle itself. We unfortunately can't merely keep a
                //    reference to the handle element because in #3 Pd will
                //    have erased it.
                // Pro: I don't have to dig into the C code to get this to
                //      work.
                // Con: It's inherently racy. #3 and #4 happen at the same
                //      time, so it's possible to apply dx/dy to the wrong
                //      handle (which will eventually get erased by Pd anyway).
                //      Anyhow, this is all very bad, but it works so it's
                //      at least not the worst of all possible worlds.
                let [new_x, new_y] = evt.type === "touchmove"
                    ? [Math.trunc(evt.touches[0].pageX),
                       Math.trunc(evt.touches[0].pageY)]
                    : [evt.pageX, evt.pageY];
                var dx = (new_x + svg_view.x) - last_draggable_x,
                    dy = (new_y + svg_view.y) - last_draggable_y,
                    handle_elem = document.querySelector(
                        draggable_label ?
                            ".move_handle" :
                            ".cnv_resize_handle"
                        ),
                    target_id = (draggable_label ? "_l" : "_s") +
                        handle_elem.parentNode.id.slice(0,-4).slice(1),
                    is_canvas_gop_rect = document.
                        getElementsByClassName("gop_drag_handle").length ?
                        true : false;

                last_draggable_x = new_x + svg_view.x;
                last_draggable_y = new_y + svg_view.y;

                pdgui.pdsend(target_id, "_motion",
                    (new_x + svg_view.x),
                    (new_y + svg_view.y));
                // now prevent scrolling while dragging
                if (evt.type === "touchmove" && evt.cancelable) {
                    evt.preventDefault();
                }
            },
            iemgui_label_mouseup: function(evt) {
                //pdgui.post("lifting the mousebutton on an iemgui label");
                // Set last state (none doesn't count as a state)
                //pdgui.post("previous state is "
                //    + canvas_events.get_previous_state());
                var label_handle = document.querySelector(".move_handle");
                var cnv_resize_handle =
                    document.querySelector(".cnv_resize_handle");
                // Restore our cursor bindings for any drag handles that
                // happen to exist
                if (label_handle) {
                    pdgui.toggle_drag_handle_cursors(label_handle,
                        true, true);
                }
                if (cnv_resize_handle) {
                    pdgui.toggle_drag_handle_cursors(cnv_resize_handle,
                        false, true);
                }
                canvas_events[canvas_events.get_previous_state()]();
            },
            hscroll_mouseup: function(evt) {
                //pdgui.post("hscroll_mouseup");
                document.getElementById("hscroll").style.setProperty("background-color", "rgba(0, 0, 0, 0.267)");
                document.getElementById("scroll_overlay").style.visibility = "hidden";
                canvas_events[canvas_events.get_previous_state()]();
                // hack-ish restoring of the cursor, since we don't know
                // what we're over until the mouse moves again, we select
                // default cursor, just to let the user know the drag is over
                document.body.style.cursor = "default";
            },
            hscroll_mousemove: function(evt) {
                //pdgui.post("hscroll_mousemove");
                if (evt.movementX != 0) {
                    //console.log("move: " + e.movementX);
                    
                    var hscroll = document.getElementById("hscroll");
                    var svg_elem = document.getElementById("patchsvg");
                    
                    var min_width = document.body.clientWidth + 3;
                    var width = svg_elem.getAttribute('width');
                    var xScrollSize;
                    
                    xScrollSize = hscroll.offsetWidth;
                  
                    var xTranslate = evt.movementX *
                        ((width - min_width)/(min_width - xScrollSize)) *
                        (evt.movementX > 0 ? 1 : 0.75);
                    if (xTranslate > 0 && xTranslate < 1) {
                        xTranslate = 1;
                    }
                    if (xTranslate < 0 && xTranslate > -1) {
                        xTranslate = -1;
                    }
                    //console.log(xTranslate);
                    window.scrollBy(xTranslate, 0);
                }
            },
            vscroll_mouseup: function(evt) {
                document.getElementById("vscroll").style.setProperty("background-color", "rgba(0, 0, 0, 0.267)");
                document.getElementById("scroll_overlay").style.visibility = "hidden";
                canvas_events[canvas_events.get_previous_state()]();
                // hack-ish restoring of the cursor, since we don't know
                // what we're over until the mouse moves again, we select
                // default cursor, just to let the user know the drag is over
                document.body.style.cursor = "default";
            },
            vscroll_mousemove: function(evt) {
                if (evt.movementY != 0) {
                    //console.log("move: " + e.movementY);
                    
                    var vscroll = document.getElementById("vscroll");
                    var svg_elem = document.getElementById("patchsvg");
                    
                    var min_height = document.body.clientHeight + 3;
                    var height = svg_elem.getAttribute('height');
                    var yScrollSize;
                    
                    yScrollSize = vscroll.offsetHeight;
                  
                    var yTranslate = evt.movementY *
                        ((height - min_height)/(min_height - yScrollSize)) *
                        (evt.movementY > 0 ? 2 : 1.5);
                    if (yTranslate > 0 && yTranslate < 1) {
                        yTranslate = 1;
                    }
                    if (yTranslate < 0 && yTranslate > -1) {
                        yTranslate = -1;
                    }
                    //console.log(yTranslate);
                    window.scrollBy(0, yTranslate);
                }
            },
            dropdown_menu_keydown: function(evt) {
                var select_elem = document.querySelector("#dropdown_list"),
                    li;
                switch(evt.keyCode) {
                    case 13:
                    case 32:
                        dropdown_index_to_pd(select_elem);
                        select_elem.style.setProperty("display", "none");
                        canvas_events.normal();
                        break;
                    case 27: // escape
                        select_elem.style.setProperty("display", "none");
                        canvas_events.normal();
                        break;
                    case 38: // up
                        li = select_elem.querySelector(".highlighted");
                        li = li.previousElementSibling ||
                             li.parentElement.lastElementChild;
                        dropdown_highlight_elem(li, "up");
                        evt.preventDefault();
                        break;
                    case 40: // down
                        li = select_elem.querySelector(".highlighted");
                        li = li.nextElementSibling ||
                             li.parentElement.firstElementChild;
                        dropdown_highlight_elem(li, "down");
                        evt.preventDefault();
                        break;
                    default:
                }
            },
            dropdown_menu_keypress: function(evt) {
                var li_nodes = document.querySelectorAll("#dropdown_list li"),
                    string_array = [],
                    highlighted,
                    highlighted_index,
                    match,
                    offset;
                highlighted = document
                    .querySelector("#dropdown_list .highlighted");
                if (highlighted) {
                    highlighted_index =
                        +document.querySelector("#dropdown_list .highlighted")
                            .getAttribute("data-index");
                    offset = highlighted_index + 1;
                } else {
                    highlighted_index = 1;
                    offset = 2;
                }
                Array.prototype.forEach.call(li_nodes, function(e, i, a) {
                    var s = a[(i + offset) % a.length];
                    string_array.push(s.textContent.trim());
                });
                match = string_array.indexOf(
                    string_array.find(function(e) {
                        return e.charAt(0).toLowerCase() ===
                            String.fromCharCode(evt.charCode).toLowerCase();
                }));
                if (match !== undefined) {
                    match = (match + offset) % li_nodes.length;
                    if (match !== highlighted_index) {
                        dropdown_highlight_elem(li_nodes[match],
                            match < highlighted_index ? "up" : "down");
                    }
                }
            },
            dropdown_menu_mousedown: function(evt) {
                //pdgui.post("dropdown_menu_mousedown " + dropdown_mousedown +
                //  " evt.type=" + evt.type);
                dropdown_mousedown = 1;
                let [pointer_x, pointer_y] = evt.type === "touchstart"
                ? [Math.trunc(evt.touches[0].pageX),
                   Math.trunc(evt.touches[0].pageY)]
                : [evt.pageX, evt.pageY];

                var select_elem = document.querySelector("#dropdown_list"),
                    in_dropdown = evt.target;
                //pdgui.post("select_elem=" + select_elem +
                //    " in_dropdown=" + in_dropdown);
                while (in_dropdown) {
                    if (in_dropdown.id === "dropdown_list") {
                        break;
                    }
                    in_dropdown = in_dropdown.parentNode;
                }
                // Allow scrollbar click and drag without closing the menu
                if (in_dropdown &&
                        pointer_x - select_elem.offsetLeft >
                        select_elem.clientWidth) {
                        //pdgui.post("in_dropdown");
                        return;
                }

                // Special case for OSX, where the scrollbar doesn't take
                // up any extra space
                if (nw.process.platform === "darwin"
                    && (evt.target.id === "dropdown_list")) {
                    return;
                }
                if (evt.target.parentNode
                    && evt.target.parentNode.parentNode
                    && evt.target.parentNode.parentNode.id === "dropdown_list") {
                    //pdgui.post("highlight target");
                    dropdown_highlight_elem(evt.target);
                }
                // if we have a properly selected element
                // here we activate and send the message to pd
                if (dropdown_index_to_pd(select_elem)) {
                    dropdown_mousedown = 0;
                    select_elem.style.setProperty("display", "none");
                    if (evt.cancelable) evt.preventDefault();
                    canvas_events.normal();
                }
            },
            dropdown_menu_mouseup: function(evt) {
                //pdgui.post("dropdown_menu_mouseup " + dropdown_mousedown +
                //  " evt.type=" + evt.type + " " + last_dropdown_menu_x + ","
                //  + last_dropdown_menu_y);
                var tgt = document.elementFromPoint(
                    last_dropdown_menu_x, last_dropdown_menu_y);
                var i, select_elem;
                // This can be triggered if the user keeps the mouse down
                // to highlight an element and releases the mouse button to
                // choose that element
                if (tgt.parentNode
                    && tgt.parentNode.parentNode
                    && tgt.parentNode.parentNode.id === "dropdown_list") {
                    //pdgui.post("...in the menu");
                    select_elem = document.querySelector("#dropdown_list");
                    dropdown_index_to_pd(select_elem);
                    select_elem.style.setProperty("display", "none");
                    canvas_events.normal();
                } else if ((evt.type === "touchend" || evt.type === "mouseup")
                        && dropdown_mousedown == 1) {
                    //pdgui.post("...outside the menu");
                    dropdown_mousedown = 0;
                    select_elem = document.querySelector("#dropdown_list");
                    select_elem.style.setProperty("display", "none");
                    canvas_events.normal();                    
                }
                if (evt.type === "touchend") {
                    //evt.stopPropagation();
                    evt.preventDefault();
                }
            },
            dropdown_menu_wheel: function(evt) {
                // Here we generate bogus mouse coords so that
                // we can break through the filter below if we're
                // using the mouse wheel to scroll in the list.
                last_dropdown_menu_x = Number.MIN_VALUE;
                last_dropdown_menu_y = Number.MIN_VALUE;
            },
            dropdown_menu_mousemove: function(evt) {
                let [pointer_x, pointer_y] = evt.type === "touchmove"
                ? [Math.trunc(evt.touches[0].pageX),
                   Math.trunc(evt.touches[0].pageY)]
                : [evt.pageX, evt.pageY];
                var tgt = document.elementFromPoint(pointer_x, pointer_y);
                //pdgui.post("dropdown_menu_mousemove evt.type=" + 
                //    evt.type + " x=" + pointer_x + " y=" + pointer_y);
                // For whatever reason, Chromium decides to trigger the
                // mousemove/mouseenter/mouseover events if the element
                // underneath it changes (or for mousemove, if the element
                // moves at all). Unfortunately that means we have to track
                // the mouse position the entire time to filter out the
                // true mousemove events from the ones Chromium generates
                // when a scroll event changes the element under the mouse.
                //pdgui.post("last:" +
                //    last_dropdown_menu_x + "," + last_dropdown_menu_y);
                //pdgui.post(document.elementFromPoint(pointer_x, pointer_y));
                var tgt = document.elementFromPoint(pointer_x, pointer_y);
                //pdgui.post(tgt.parentNode);
                //pdgui.post(tgt.parentNode.parentNode);
                //pdgui.post(tgt.parentNode.parentNode.id);
                if (pointer_x !== last_dropdown_menu_x
                    || pointer_y !== last_dropdown_menu_y) {
                    if (tgt.parentNode
                        && tgt.parentNode.parentNode
                        && tgt.parentNode.parentNode.id === "dropdown_list") {
                        //pdgui.post("highlight");
                        dropdown_highlight_elem(tgt);
                    } else {
                        dropdown_clear_highlight();
                    }
                }
                last_dropdown_menu_x = pointer_x;
                last_dropdown_menu_y = pointer_y;
                if (evt.cancelable) evt.preventDefault();
            }
        },
        changes_in_completion_index = 0,
        utils = {
            create_obj: function() {
                // Yes: I _really_ want .innerText and NOT .textContent
                // here.  I want those newlines: although that isn't
                // standard in Pd-Vanilla, Pd-l2ork uses and preserves
                // them inside comments
                var iscomment = textbox().getAttribute("type");

                // ico@vt.edu 2022-11-17: fix issue of erroneous
                // addition of lines (only for comments) because
                // <div><br></div> is interpreted as two lines
                // by the innerText. So, we change it to
                // <div> </div> by replacing <br> with " ".
                // an example of this issue is visible if you
                // create a comment with the following:
                //
                // abc\n
                // \n
                // def
                //
                // this will create one extra erroneous empty line
                if (iscomment)
                    textbox().innerHTML = textbox().innerHTML.replace(/<br>/g, " ");

                //pdgui.post("===========\ntextbox=<" + textbox().innerHTML + ">");
                var fudi_msg = text_to_fudi(textbox().innerText,
                        (iscomment === "comment" ? 1 : 0)),
                    fudi_array = string_to_array_of_chunks(fudi_msg),
                    i;
                for (i = 0; i < fudi_array.length; i++) {
                    //pdgui.post("adding: " + fudi_array[i]);
                    pdgui.pdsend(name, "obj_addtobuf", fudi_array[i]);
                }
                pdgui.pdsend(name, "obj_buftotext");

                // GB: index created object
                let obj = document.getElementById(textbox().getAttribute("tag")+"gobj");
                // find obj class and remove word "selected"
                let obj_class = obj.getAttribute("class").toString().split(" ").slice(0,1).toString();
                if (pdgui.autocomplete_enabled()) {
                    pdgui.index_obj_completion(obj_class, fudi_msg);
                    // GB: save every 50 changes, so that we don't loose too
                    // much data in case pd-l2ork unexpectedly quits or crashes
                    if (changes_in_completion_index===0 ||
                        ++changes_in_completion_index > 50) {
                        pdgui.write_completion_index();
                        changes_in_completion_index = 1;
                    }
                }
            }
        }
    ;

    return {
        none: function() {
            var evt_name, prop;
            if (state !== "none") {
                previous_state = state;
            }
            state = "none";
            for (prop in events) {
                if (events.hasOwnProperty(prop)) {
                    evt_name = prop.split("_");
                    evt_name = evt_name[evt_name.length - 1];
                    document.removeEventListener(evt_name, events[prop], false);
                    // ag: Also get rid of the touch event handlers, which are
                    // bound to the same handlers as the corresponding mouse
                    // events.
                    if (evt_name == "mousemove") {
                        document.removeEventListener("touchmove",
                                                     events[prop], false);
                    } else if (evt_name == "mousedown") {
                        document.removeEventListener("touchstart",
                                                     events[prop], false);
                    } else if (evt_name == "mouseup") {
                        document.removeEventListener("touchend",
                                                     events[prop], false);
                    }
                }
            }
        },
        normal: function() {
            //pdgui.post("normal");
            canvas_events.none();

            document.addEventListener("mousemove", events.mousemove, false);
            document.addEventListener("touchmove", events.mousemove, {passive:false});
            document.addEventListener("keydown", events.keydown, false);
            document.addEventListener("keypress", events.keypress, false);
            document.addEventListener("keyup", events.keyup, false);
            document.addEventListener("mousedown", events.mousedown, false);
            document.addEventListener("touchstart", events.mousedown, {passive:false});
            document.addEventListener("mouseup", events.mouseup, false);
            document.addEventListener("touchend", events.mouseup, {passive:false});
            state = "normal";
            set_menu_modals(name, true);
        },
        // hlkwok@vt.edu: 2022-11-24: Disallow clicking objects through menu
        k12menu: function() {
            // ico@vt.edu 2022-12-08: have to approach this in a hybrid fashion
            // we disable all events, then enable select ones, and then make
            // our state "normal". otherwise events dependent on the normal
            // state will not work (some of them have checks inside functions).
            canvas_events.none();
            document.addEventListener("keydown", events.keydown, false);
            document.addEventListener("keypress", events.keypress, false);
            document.addEventListener("keyup", events.keyup, false);
            document.addEventListener("mouseup", events.mouseup, false);
            //document.addEventListener("mousemove", events.mousemove, false);
            state = "normal";
            toggle_put_menu(false);
            // ico@vt.edu 2022-12-08: we do this to prevent scroll from
            // "leaking" onto the patchsvg below the k12 menu
            // ico@vt.edu 2022-12-16: this is problematic because
            // leaving event is not always triggered accurately,
            // in part because of the animated menu
            //document.body.style.overflow = 'hidden';
            //pdgui.post("k12menu activated");
        },
        k12menu_out: function() {
            //pdgui.post("k12menu_out activated");
            //document.body.style.overflow = 'visible';
            // ico@vt.edu 2022-12-08: we do this to reenable patchsvg
            // scrolling
            toggle_put_menu(true);
            // ico@vt.edu 2022-12-16: this is problematic because
            // leaving event is not always triggered accurately,
            // in part because of the animated menu
            //document.body.style.overflow = 'visible';
            canvas_events.normal();
        },
        scalar_drag: function() {
            // This scalar_drag is a prototype for moving more of the editing
            // environment directly to the GUI.  At the moment we're leaving
            // the other "normal" events live, since behavior like editmode
            // selection still happens from the Pd engine.
            //this.none();
            document.addEventListener("mousemove", events.scalar_draggable_mousemove, false);
            document.addEventListener("touchmove", events.scalar_draggable_mousemove, false);
            document.addEventListener("mouseup", events.scalar_draggable_mouseup, false);
            document.addEventListener("touchend", events.scalar_draggable_mouseup, false);
        },
        iemgui_label_drag: function() {
            //pdgui.post("iemgui_label_drag");
            // This is a workaround for dragging iemgui labels. Resizing iemguis
            // currently happens in Pd (canvas_doclick and canvas_motion). (Look
            // for MA_RESIZE.)
            // The exception is my_canvas, which is weird because the visible
            // rectangle extends past the bbox that it reports to Pd.
            // Unfortunately that means a lot of work to treat it separately.
            canvas_events.none();
            document.addEventListener("mousemove",
                events.iemgui_label_mousemove, false);
            document.addEventListener("touchmove",
                events.iemgui_label_mousemove, {passive:false});
            document.addEventListener("mouseup",
                events.iemgui_label_mouseup, false);
            document.addEventListener("touchend",
                events.iemgui_label_mouseup, false);
        },
        hscroll_drag: function() {
            //pdgui.post("hscroll_drag");
            canvas_events.none();
            document.getElementById("hscroll").style.cssText += "background-color: rgba(0, 0, 0, 0.5) !important";
            document.getElementById("scroll_overlay").style.visibility = "visible";
            document.addEventListener("mouseup", events.hscroll_mouseup, false);
            document.addEventListener("mousemove", events.hscroll_mousemove, false);
        },
        vscroll_drag: function() {
            canvas_events.none();
            document.getElementById("vscroll").style.cssText += "background-color: rgba(0, 0, 0, 0.5) !important";
            document.getElementById("scroll_overlay").style.visibility = "visible";
            document.addEventListener("mouseup", events.vscroll_mouseup, false);
            document.addEventListener("mousemove", events.vscroll_mousemove, false);
        },
        text: function() {
            //pdgui.post("text");
            canvas_events.none();

            document.addEventListener("mousemove", events.text_mousemove, false);
            document.addEventListener("keydown", events.text_keydown, false);
            document.addEventListener("keypress", events.text_keypress, false);
            document.addEventListener("keyup", events.text_keyup, false);
            document.addEventListener("paste", events.text_paste, false);
            document.addEventListener("mousedown", events.text_mousedown, false);
            document.addEventListener("mouseup", events.text_mouseup, false);
            state = "text";
            set_edit_menu_modals(name, false);
        },
        floating_text: function() {
            //pdgui.post("floating_text");
            canvas_events.none();
            canvas_events.text();
            document.removeEventListener("mousedown", events.text_mousedown, false);
            document.removeEventListener("mouseup", events.text_mouseup, false);
            document.removeEventListener("keypress", events.text_keypress, false);
            document.removeEventListener("mousemove", events.text_mousemove, false);
            document.addEventListener("click", events.floating_text_click, false);
            document.addEventListener("keypress", events.floating_text_keypress, false);
            document.addEventListener("mousemove", events.mousemove, false);
            state = "floating_text";
            set_edit_menu_modals(name, false);
        },
        dropdown_menu: function() {
            canvas_events.none();
          
            document.addEventListener("mousedown", events.dropdown_menu_mousedown, false);
            document.addEventListener("touchstart", events.dropdown_menu_mousedown, {passive:false});
            
            document.addEventListener("mouseup", events.dropdown_menu_mouseup, false);
            document.addEventListener("touchend", events.dropdown_menu_mouseup, {passive:false});
            
            document.addEventListener("mousemove", events.dropdown_menu_mousemove, false);
            document.addEventListener("touchmove", events.dropdown_menu_mousemove, {passive:false});
            
            document.addEventListener("keydown", events.dropdown_menu_keydown, false);
            
            document.addEventListener("keypress", events.dropdown_menu_keypress, false);
            
            document.querySelector("#dropdown_list")
                .addEventListener("wheel", events.dropdown_menu_wheel, false);
        },
        search: function() {
            //pdgui.post("search");
            canvas_events.none();
            document.addEventListener("keydown", events.find_keydown, false);
            state = "search";
        },
        update_scrollbars: function() {
            pdgui.gui_update_scrollbars(name);
        },
        register: function(n) {
            name = n;
        },
        get_id: function() {
            return name;
        },
        get_state: function() {
            return state;
        },
        get_previous_state: function() {
            return previous_state;
        },
        create_obj: function() {
            utils.create_obj();
        },
        match_words: function(state) {
            match_words_state = state;
        },
        get_match_words: function() {
            return match_words_state;
        },
        find_reset: function() {
            //pdgui.post("find_reset");
            last_search_term = "";
        },
        get_last_search_term: function() {
            //pdgui.post("get_last_search_term");
            if (last_search_term !== "")
                return last_search_term;
            else return pdgui.gui_get_backend_search_term();
        },
        set_last_search_term: function(value) {
            //pdgui.post("set_last_search_term");
            last_search_term = value;
            pdgui.gui_set_backend_search_term(value);
        },
        add_scalar_draggable: function(cid, tag, scalar_sym, drawcommand_sym,
            event_name, array_sym, index) {
            scalar_draggables[tag] = {
                cid: cid,
                scalar_sym: scalar_sym,
                drawcommand_sym: drawcommand_sym,
                event_name: event_name,
                array_sym: array_sym,
                index: index
            };
        },
        remove_scalar_draggable: function(id) {
            if (scalar_draggables[id]) {
                scalar_draggables[id] = null;
            }
        },
        save_and_close: function() {
            pdgui.pdsend(name, "menusave", 1);
        },
        close_without_saving: function(cid, force) {
            pdgui.pdsend(name, "dirty 0");
            pdgui.pdsend(cid, "menuclose", force);
        },
        close_save_dialog: function() {
            document.getElementById("save_before_quit").close();
        },
        paste_from_pd_file: function(name, clipboard_data) {
            var line, lines, i, j, pd_message, pd_submessages;
            // This lets the user copy some Pd source file from another
            // application and paste the code directly into a canvas window
            // (empty or otherwise). It does a quick check to make sure the OS
            // clipboard is holding text that could conceivably be Pd source
            // code. But that's not 100% foolproof and the engine might crash
            // and burn, so be careful! :)

            // We only want to check the external buffer and/or send a "paste"
            // event to Pd if the canvas is in a "normal" state.
            if (canvas_events.get_state() !== "normal") {
                return;
            }
            if (!might_be_a_pd_file(clipboard_data)) {
                pdgui.post("paste error: clipboard doesn't appear to contain valid Pd code");
                return;
            }

            // clear the buffer
            pdgui.pdsend(name, "copyfromexternalbuffer");
            pd_message = "";
            lines = clipboard_data.split("\n");
            for (i = 0; i < lines.length; i++) {
                line = lines[i];
                //pdgui.post("copyfromexternalbuffer " + lines.length + " <" + line + ">" +
                //    " -3=<" + line.slice(-3, -2) + "> -2=<" + line.slice(-2, -1) + ">");
                // process pd_message if it ends with a semicolon that
                // isn't preceded by a backslash
                // ico 2021-11-18: differentiate between windows and *nix
                // string length and adjust slice values accordingly. it appears
                // only the first line is affected on windows...
                if (
                    (pdgui.nw_os_is_windows && line.slice(-2, -1) === ';' &&
                    (line.length < 2 || line.slice(-3, -2) !== '\\'))
                    ||
                    (line.slice(-1) === ";" &&
                    (line.length < 2 || line.slice(-2, -1) !== "\\"))
                   ) {
                    if (pd_message === "") {
                        pd_message = line;
                    } else {
                        pd_message = pd_message + " " + line;
                    }
                    // ico 2021-11-18: convert the defunct format:
                    //   object args, f <num> \;
                    // to
                    //   object args \;
                    //   #X f <num> \;
                    //pd_message = pd_message.replace(/(?<!\\),/g, ';\n#X');
                    pd_submessages = pd_message.match(/(?:\\,|[^,])+/g);
                    if (pd_submessages.length == 2) {
                        pd_submessages[0] = pd_submessages[0] + ";";
                        pd_submessages[1] = "#X" + pd_submessages[1];
                    }
                    for (j = 0; j < pd_submessages.length; j++) {
                        //pdgui.post("...{" + pd_submessages[j] + "}");
                        pdgui.pdsend(name, "copyfromexternalbuffer", pd_submessages[j]);
                    }
                    //pd_message = pd_message.replace(/;/g, "\\;");

                    //pdgui.post("sending to backend line <"+ pd_message +">");
                    //pdgui.pdsend(name, "copyfromexternalbuffer", pd_message);
                    pd_message = "";
                    pd_submessages = null;
                } else {
                    pd_message = pd_message + " " + line;
                    pd_message = pd_message.replace(/\n/g, "");
                    //pdgui.post("else <" + pd_message + ">");
                }
            }
            // This signals to the engine that we're done filling the external
            // buffer, that it can do necessary checks and call some internal
            // cleanup code.

            pdgui.pdsend(name, "copyfromexternalbuffer");
            // Send a canvas "paste" message to Pd
            pdgui.pdsend(name, "paste");
            // Finally, make sure to reset the buffer so that next time around
            // we start from a clean slate. (Otherwise, the engine will just
            // add stuff to the existing buffer contents.)
            pdgui.pdsend(name, "reset_copyfromexternalbuffer");
        },
        init: function() {
            document.getElementById("saveDialog")
                .setAttribute("nwworkingdir", pdgui.get_pwd());
            document.getElementById("fileDialog")
                .setAttribute("nwworkingdir", pdgui.get_pwd());
            document.getElementById("fileDialog").setAttribute("accept",
                Object.keys(pdgui.pd_filetypes).toString());
            // Dialog events -- these are set elsewhere now because of a bug
            // with nwworkingdir
            document.querySelector("#saveDialog").addEventListener("change",
                function(evt) {
                    pdgui.saveas_callback(name, evt.target.value, 0);
                    // reset value so that we can open the same file twice
                    evt.target.value = null;
                    console.log("tried to save something");
                }, false
            );
                       
            // add listener for the scrollbars
            document.getElementById("hscroll").
                addEventListener("mousedown", canvas_events.hscroll_drag, false);
            document.getElementById("vscroll").
                addEventListener("mousedown", canvas_events.vscroll_drag, false);
            
            // Whoa-- huge workaround! Right now we're getting
            // the popup menu the way Pd Vanilla does it:
            // 1) send a mouse(down) message to Pd
            // 2) Pd checks whether it wants to send us a popup
            // 3) Pd checks what popup menu items are available for obj/canvas
            // 4) Pd sends GUI back a message with this info
            // 5) GUI finally displays the popup
            // 6) GUI keeps a _global_ _variable_ to remember the popup coords
            // 7) User clicks an option in the popup
            // 8) GUI sends a message back to Pd with the popup index and coords
            // 9) Pd walks the linked list of objects to look up the object
            // 10) Pd asks the object if it reacts to popups, and if it reacts
            //     to the selected item in the popup
            // 11) Pd sends a message to the relevant object for the item in
            //     question
            // nw.js has a nice little "contextmenu" event handler, but it's too
            // difficult to use when passing between GUI and Pd (twice). In the
            // future we should do all popup menu event handling in the GUI,
            // and only pass a message to Pd when the user has clicked an item.
            // For now, however, we just turn off its default behavior and
            // control it with a bunch of complicated callbacks.
            document.addEventListener("contextmenu", function(evt) {
                //console.log("got a context menu evt...");
                evt.stopPropagation();
                evt.preventDefault();
            });

            // Cut event
            document.addEventListener("cut", function(evt) {
                // On OSX, this event gets triggered when we're editing
                // inside an object/message box, including activated objects.
                // ico@vt.edu 2021-10-20: we don't need the search part since
                // that one for some reason works just fine on OSX (see
                // m.edit.copy above)
                if (pdgui.gui_is_gobj_grabbed(name)) {
                    // ico@vt.edu 2021-10-08: cutting contents of a grabbed object
                    //pdgui.post("gobj_grabbed copy");
                    var clipboard = nw.Clipboard.get();
                    var activated_gobj =
                        document.getElementsByClassName("activated");
                    // currently we only support cutting from gatoms and number2
                    // gatoms will belong to the class atom, while number2 will have
                    // parent object that will belong to the class iemgui
                    if (activated_gobj[0].classList.contains("atom"))
                        clipboard.set(activated_gobj[0].textContent, 'text');
                    else if (activated_gobj[0].parentNode.classList.contains("iemgui")) {
                        var output = activated_gobj[0].textContent.replace('>', '');
                        clipboard.set(output, 'text');
                    }
                    for (var i = 0; i < clipboard.get('text').length; i++) {
                        // Send backspace
                        pdgui.canvas_sendkey(name, 1, 0, 8, 0);
                        pdgui.canvas_sendkey(name, 0, 0, 8, 0);
                    }
                } else if (canvas_events.get_state() === "normal") {
                    // So, we only forward the copy message to Pd if we're in a
                    // "normal" canvas state
                    pdgui.pdsend(name, "cut");
                }
            });

            // Copy event
            document.addEventListener("copy", function(evt) {
                // On OSX, this event gets triggered when we're editing
                // inside an object/message box, including activated objects.
                // ico@vt.edu 2021-10-20: we don't need the search part since
                // that one for some reason works just fine on OSX (see
                // m.edit.copy above)
                if (pdgui.gui_is_gobj_grabbed(name)) {
                    // ico@vt.edu 2021-10-08: copying contents of a grabbed object
                    //pdgui.post("gobj_grabbed copy");
                    var clipboard = nw.Clipboard.get();
                    var activated_gobj =
                        document.getElementsByClassName("activated");
                    // currently we only support copying from gatoms and number2
                    // gatoms will belong to the class atom, while number2 will have
                    // parent object that will belong to the class iemgui
                    if (activated_gobj[0].classList.contains("atom"))
                        clipboard.set(activated_gobj[0].textContent, 'text');
                    else if (activated_gobj[0].parentNode.classList.contains("iemgui")) {
                        var output = activated_gobj[0].textContent.replace('>', '');
                        clipboard.set(output, 'text');
                    }
                } else if (canvas_events.get_state() === "normal") {
                    // So, we only forward the copy message to Pd if we're in a
                    // "normal" canvas state
                    pdgui.pdsend(name, "copy");
                }
            });

            // Listen to paste event
            // ico@vt.edu 2021-09-28: This is used by OSX only?, so we
            // redundantly reproduce here the paste when gatom has been grabbed
            document.addEventListener("paste", function(evt) {
                // ico@vt.edu 2021-10-20: we don't need the search part since
                // that one for some reason works just fine on OSX (see m.edit.paste
                // above)
                if (canvas_events.get_state() !== "normal") {
                    return;
                }
                if (pdgui.gui_is_gobj_grabbed(name)) {
                    // ico@vt.edu 2021-09-28: pasting inside a grabbed object
                    //pdgui.post("gobj_grabbed paste <" + nw.Clipboard.get().get('text') + ">");
                    var paste_text = nw.Clipboard.get().get('text');
                    for (var i = 0; i < paste_text.length; i++) {
                        pdgui.canvas_sendkey(name, 1, 0, paste_text.charCodeAt(i), 0);
                        pdgui.canvas_sendkey(name, 0, 0, paste_text.charCodeAt(i), 0);
                    }
                } else {
                    // Send a canvas "paste" message to Pd
                    pdgui.pdsend(name, "paste");
                }
            });
            document.getElementById("k12_menu").addEventListener("wheel", function(evt) {
                var d = { deltaX: 0, deltaY: 0, deltaZ: 0 };
                Object.keys(d).forEach(function(key) {
                    if (evt[key] < 0) {
                        d[key] = -1;
                    } else if (evt[key] > 0) {
                        d[key] = 1;
                    } else {
                        d[key] = 0;
                    }
                });
                pdgui.pdsend(name, "legacy_mousewheel",
                    d.deltaX, d.deltaY, d.deltaZ);
                //pdgui.post("k12_menu scroll " + d.deltaY);
                var tab = document.getElementById("tab_menu");
                tab.scroll(0, tab.scrollTop + (evt.deltaY));
                evt.preventDefault();
                //evt.stopPropagation();
            });

            // MouseWheel event for zooming
            document.addEventListener("wheel", function(evt) {
                var d = { deltaX: 0, deltaY: 0, deltaZ: 0 };
                Object.keys(d).forEach(function(key) {
                    if (evt[key] < 0) {
                        d[key] = -1;
                    } else if (evt[key] > 0) {
                        d[key] = 1;
                    } else {
                        d[key] = 0;
                    }
                });
                // Send a message on to Pd for the [mousewheel] legacy object
                // (in the future we can refcount to prevent forwarding
                // these messages when there's no extant receiver)
                pdgui.pdsend(name, "legacy_mousewheel",
                    d.deltaX, d.deltaY, d.deltaZ);

                // ico@vt.edu 2022-12-16: catching wheel inertia on Windows
                // to prevent unwanted zooms once one presses ctrl after having
                // done a large swipe with two fingers to scroll
                var threshold = 10 + (pdgui.nw_os_is_osx ? 590 : 0);
                if (evt.deltaY > -threshold && evt.deltaY < threshold && 
                    evt.deltaX > -threshold && evt.deltaX < threshold) {
                    //pdgui.post("minimal delta");
                    return;
                }
                if (pdgui.cmd_or_ctrl_key(evt)) {
                    // scroll up for zoom-in, down for zoom-out
                    nw_window_zoom(name, -d.deltaY);
                }
            });

            // ico@vt.edu 2022-12-16: this on windows prevents
            // pinch resizing using default zoom levels and thus
            // matches behavior on OSX
            window.visualViewport.addEventListener("resize", function(evt) {
                //pdgui.post("viewport resize");
                evt.preventDefault();
            });

            // The following is commented out because we have to set the
            // event listener inside nw_create_pd_window_menus due to a
            // bug with nwworkingdir

            //document.querySelector("#fileDialog").addEventListener("change",
            //    function(evt) {
            //        var file_array = this.value;
            //        // reset value so that we can open the same file twice
            //        this.value = null;
            //        pdgui.menu_open(file_array);
            //        console.log("tried to open something\n\n\n\n\n\n\n\n");
            //    }, false
            //);
            document.querySelector("#openpanel_dialog")
                .addEventListener("change", function(evt) {
                    var file_string = evt.target.value;
                    // reset value so that we can open the same file twice
                    evt.target.value = null;
                    pdgui.file_dialog_callback(file_string);
                    console.log("tried to openpanel something");
                }, false
            );
            document.querySelector("#savepanel_dialog")
                .addEventListener("change", function(evt) {
                    var file_string = evt.target.value;
                    // reset value so that we can open the same file twice
                    evt.target.value = null;
                    pdgui.file_dialog_callback(file_string);
                    console.log("tried to savepanel something");
                }, false
            );
            document.querySelector("#canvas_find")
                .addEventListener("mousedown", events.find_ignore, false
            );
            document.querySelector("#canvas_find_text")
                .addEventListener("mousedown", canvas_events.search, false
            );

            // disable drag and drop for the time being
            // ico@vt.edu 2022-12-07: added dragstart and drop to the mix
            // which appears to solve occasional hscroll and vscroll drag
            // and results in multidimensional scroll... nw.js 0.67.1
            window.addEventListener("dragstart", function (evt) {
                evt.preventDefault();
            }, false);
            window.addEventListener("drop", function (evt) {
                evt.preventDefault();
            }, false);
            window.addEventListener("dragover", function (evt) {
                evt.preventDefault();
            }, false);
            window.addEventListener("drop", function (evt) {
                evt.preventDefault();
            }, false);

            // Add placeholder text... this all needs to be collected into an 
            // add_events function similiar to the one in index.js
            document.querySelector("#canvas_find_text").placeholder =
                l("canvas.find.placeholder");
            document.querySelector("#canvas_find_text").addEventListener("blur",
                canvas_events.normal, false
            );
            document.querySelector("#canvas_find_button")
                .addEventListener("mousedown", events.find_click
            );
            document.querySelector("#canvas_find_whole_word")
                .addEventListener("mousedown", events.match_words
            );
            // We need to separate these into nw_window events and html5 DOM
            // events closing the Window this isn't actually closing the window
            // yet
            gui.Window.get().on("close", function(arg) {
                pdgui.canvas_check_geometry(name);
                pdgui.pdsend(name, "menuclose 0");
                if (arg === "quit") {
                    pdgui.menu_quit();
                }
            });
            // update viewport size when window size changes
            gui.Window.get().on("maximize", function() {
                pdgui.gui_canvas_get_scroll(name);
                update_k12_menu();
            });
            gui.Window.get().on("restore", function() {
                pdgui.gui_canvas_get_scroll(name);
                update_k12_menu();
            });
            gui.Window.get().on("resize", function() {
                //pdgui.post("window resize");
                //pdgui.gui_canvas_get_scroll(name);
                pdgui.gui_canvas_get_scroll_on_resize(name);
                // hlkwok@vt.edu 2022-11-8: update k12 menu when
                // window resizes
                // ico@vt.edu 2022-12-05: add a delay, so that
                // when switching to full screen, we are sure
                // we got the right size
                setTimeout(function() { update_k12_menu() }, 0);
            });
            gui.Window.get().on("focus", function() {
                //pdgui.post("focus: patch");
                nw_window_focus_callback(name);
            });            
            gui.Window.get().on("blur", function() {
                nw_window_blur_callback(name);
            });
            gui.Window.get().on("move", function(x, y) {
                var w = gui.Window.get();
                // ico@vt.edu 2022-12-02:
                // here we subtract 22 since somehow window size
                // grows on subsequent checks. it is not the menu
                // since it also affects osx, and it should not
                // be the titlebar since it is different on
                // different OSs. Affects 0.67.1, although I seem
                // to recall seeing it before, as well.
                pdgui.pdsend(name, "setbounds", x, y,
                    x + w.width, y + w.height - 22);
            });
            
            // map onscroll event
            document.addEventListener("scroll", function() {
                pdgui.gui_update_scrollbars(name);
            });
            
            // set minimum window size
            gui.Window.get().setMinimumSize(150, 100);
        }
    };
}());

// Stop-gap translator. We copy/pasted this in each dialog, too. It
// should be moved to pdgui.js
function translate_form() {
    var i
    var elements = document.querySelectorAll("[data-i18n]");
    for (i = 0; i < elements.length; i++) {
        var data = elements[i].dataset.i18n;
        if (data.slice(0,7) === "[title]") {
            elements[i].title = l(data.slice(7));
        } else {
            elements[i].textContent = l(data);
        }
    }
}

// This gets called from the nw_create_window function in index.html
// It provides us with our canvas id from the C side. Once we have it
// we can create the menu and register event callbacks
function register_window_id(cid, attr_array) {
    //pdgui.post("register_window_id isblank=" + attr_array.isblank);
    var kludge_title;
    // We create the window menus and popup menu before doing anything else
    // to ensure that we don't try to set the svg size before these are done.
    // Otherwise we might set the svg size to the window viewport, only to have
    // the menu push down the svg viewport and create scrollbars. Those same
    // scrollbars will get erased once canvas_map triggers, causing a quick
    // (and annoying) scrollbar flash.
    // For OSX, we have a single menu and just track which window has the
    // focus.
    if (process.platform !== "darwin") {
        nw_create_patch_window_menus(gui, cid);
    }
    create_popup_menu(cid);
    canvas_events.register(cid);
    // Initialize global DOM state/events
    canvas_events.init(document);
    translate_form();
    // Trigger a "focus" event so that OSX updates the menu for this window
    nw_window_focus_callback(cid);
    canvas_events.normal();
    // Initialize the zoom level to the value retrieved from the patch, if any.
    nw.Window.get().zoomLevel = attr_array.zoom;
    pdgui.canvas_map(cid); // side-effect: triggers gui_canvas_get_scroll
    pdgui.canvas_query_editmode(cid);
    // For now, there is no way for the cord inspector to be turned on by
    // default. But if this changes we need to set its menu item checkbox
    // accordingly here
    // set_cord_inspector_checkbox();

    // Set scroll bars
    pdgui.canvas_set_scrollbars(cid, !attr_array.hide_scroll);
    // One final kludge-- because window creation is asynchronous, we may
    // have gotten a dirty flag before the window was created. In that case
    // we check the title_queue to see if our title now contains an asterisk
    // (which is the visual cue for "dirty")

    // Enable/disable the warning for multiple dirty instances
    pdgui.gui_canvas_warning(cid, attr_array.warid);

    // Two possibilities for handling this better:
    // have a representation of canvas attys in pdgui.js (editmode, dirty, etc.)
    // or
    // send those attys from Pd after mapping the canvas
    kludge_title = pdgui.query_title_queue(cid);
    if (kludge_title) {
        nw.Window.get().title = kludge_title;
    }
    pdgui.free_title_queue(cid);
    document.body.addEventListener("load", update_menu_items(cid, attr_array.isblank), false);
}

function create_popup_menu(name) {
    // The right-click popup menu
    var popup_menu = new gui.Menu();
    pdgui.add_popup(name, popup_menu);
    // hlkwok@vt.edu 2022-11-13: if in k12 mode, only show help option
    if (pdgui.get_k12_mode()) {
    // For some reason, menu must have at least 3 elements
    // or else it won't display
        popup_menu.append(new gui.MenuItem({
            type: "separator",
        }));
        popup_menu.append(new gui.MenuItem({
            type: "separator",
        }));
        popup_menu.append(new gui.MenuItem({
            type: "separator",
        }));
        popup_menu.append(new gui.MenuItem({
            label: l("canvas.menu.help"),
            click: function() {
                pdgui.popup_action(name, 2);
            }
        }));
        return;
    }
    popup_menu.append(new gui.MenuItem({
        label: l("canvas.menu.props"),
        click: function() {
            pdgui.popup_action(name, 0);
        }
    }));
    popup_menu.append(new gui.MenuItem({
        label: l("canvas.menu.open"),
        click: function() {
            pdgui.popup_action(name, 1);
        }
    }));
    popup_menu.append(new gui.MenuItem({
        label: l("canvas.menu.saveas"),
        click: function() {
            pdgui.popup_action(name, 5);
        }
    }));
    popup_menu.append(new gui.MenuItem({
        label: l("canvas.menu.help"),
        click: function() {
            pdgui.popup_action(name, 2);
        }
    }));
    popup_menu.append(new gui.MenuItem({
        type: "separator",
    }));
    popup_menu.append(new gui.MenuItem({
        label: l("canvas.menu.front"),
        click: function() {
            pdgui.popup_action(name, 3);
        }
    }));
    popup_menu.append(new gui.MenuItem({
        label: l("canvas.menu.back"),
        click: function() {
            pdgui.popup_action(name, 4);
        }
    }));
}

function nw_undo_menu(undo_text, redo_text) {
    // Disabling undo/redo menu buttons on OSX will
    // turn off any DOM events associated with that
    // key command. So we can't disable them here--
    // otherwise it would turn off text-editing
    // undo/redo.
    if (process.platform !== "darwin" && undo_text === "no") {
        m.edit.undo.enabled = false;
    } else {
        m.edit.undo.enabled = true;
        m.edit.undo.label = l("menu.undo") + " " + undo_text;
    }
    if (process.platform !== "darwin" && redo_text === "no") {
        m.edit.redo.enabled = false;
    } else {
        m.edit.redo.enabled = true;
        m.edit.redo.label = l("menu.redo") + " " + redo_text;
    }
}

function have_live_box() {
    var state = canvas_events.get_state();
    if (state === "text" || state === "floating_text") {
        return true;
    } else {
        return false;
    }
}

// If there's a box being edited, send the box's text to Pd
function update_live_box() {
    if (have_live_box()) {
        canvas_events.create_obj();
    }
}

// If there's a box being edited, try to instantiate it in Pd
function instantiate_live_box() {
    if (have_live_box()) {
        canvas_events.create_obj();
    }
}

// ico@vt.edu 2022-12-15: we toggle off creation of new put
// menu objects
function toggle_put_menu(state) {
    // we do this only if we are NOT in k12 mode
    // since in that mode we don't have the put
    // menu to begin with. otherwise, we trigger
    // an error and break the front-end.
    if (!pdgui.get_k12_mode()) {
        m.put.object.enabled = state;
        m.put.message.enabled = state;
        m.put.number.enabled = state;
        m.put.symbol.enabled = state;
        m.put.list.enabled = state;
        m.put.comment.enabled = state;
        m.put.dropdown.enabled = state;
        m.put.bang.enabled = state;
        m.put.toggle.enabled = state;
        m.put.number2.enabled = state;
        m.put.vslider.enabled = state;
        m.put.hslider.enabled = state;
        m.put.knob.enabled = state;
        m.put.vradio.enabled = state;
        m.put.hradio.enabled = state;
        m.put.vu.enabled = state;
        m.put.cnv.enabled = state;
        m.put.image.enabled = state;
        //m.put.graph.enabled = state;
        m.put.array.enabled = state;
    }
}

function update_osx_k12_menu(window) {
    if (pdgui.nw_os_is_osx) {
        if (pdgui.get_k12_mode())
        {
            //pdgui.post("osx k12");
            m.file.message.enabled = false;
            m.file.save.enabled =
                (pdgui.canvas_menu_get_editable(window) ? true : false);
            m.file.saveas.enabled =
                (pdgui.canvas_menu_get_editable(window) ? true : false);

            m.edit.paste_clipboard.enabled = false;
            m.edit.reselect.enabled = false;
            m.edit.clear_console.enabled = true;
            m.edit.encapsulate.enabled = false;
            m.edit.font.enabled = false;
            m.edit.find.enabled = false;
            m.edit.findagain.enabled = false;
            m.edit.finderror.enabled = false;

            m.put.object.enabled = false;
            m.put.message.enabled = false;
            m.put.number.enabled = false;
            m.put.symbol.enabled = false;
            m.put.list.enabled = false;
            m.put.comment.enabled = false;
            m.put.dropdown.enabled = false;
            m.put.bang.enabled = false;
            m.put.toggle.enabled = false;
            m.put.number2.enabled = false;
            m.put.vslider.enabled = false;
            m.put.hslider.enabled = false;
            m.put.knob.enabled = false;
            m.put.vradio.enabled = false;
            m.put.hradio.enabled = false;
            m.put.vu.enabled = false;
            m.put.cnv.enabled = false;
            m.put.image.enabled = false;
            //m.put.graph.enabled = false;
            m.put.array.enabled = false;
            m.put.k12_menu.enabled = false;

            m.media.audio_on.enabled = false;
            m.media.audio_off.enabled = false;
            m.media.test.enabled = false;
            m.media.loadmeter.enabled = false;

            m.win.nextwin.enabled = false;
            m.win.prevwin.enabled = false;
            m.win.parentwin.enabled = false;
            m.win.visible_ancestor.enabled = false;
            m.win.pdwin.enabled = true;
            m.win.abstractions.enabled = false;

            m.help.about.enabled = false;
            m.help.manual.enabled = false;
            m.help.browser.enabled = false;
            m.help.intro.enabled = false;
            m.help.l2ork_list.enabled = false;
            m.help.pd_list.enabled = false;
            m.help.forums.enabled = false;
            m.help.irc.enabled = false;
            m.help.devtools.enabled = false;
        } else if (!pdgui.canvas_menu_get_editable(window)) {
            //pdgui.post("osx not editable");
            m.file.message.enabled = true;
            m.file.save.enabled = false;
            m.file.saveas.enabled = false;

            m.edit.paste_clipboard.enabled = false;
            m.edit.reselect.enabled = false;
            m.edit.clear_console.enabled = true;
            m.edit.encapsulate.enabled = false;
            m.edit.font.enabled = false;
            m.edit.find.enabled = false;
            m.edit.findagain.enabled = false;
            m.edit.finderror.enabled = false;

            m.put.object.enabled = false;
            m.put.message.enabled = false;
            m.put.number.enabled = false;
            m.put.symbol.enabled = false;
            m.put.list.enabled = false;
            m.put.comment.enabled = false;
            m.put.dropdown.enabled = false;
            m.put.bang.enabled = false;
            m.put.toggle.enabled = false;
            m.put.number2.enabled = false;
            m.put.vslider.enabled = false;
            m.put.hslider.enabled = false;
            m.put.knob.enabled = false;
            m.put.vradio.enabled = false;
            m.put.hradio.enabled = false;
            m.put.vu.enabled = false;
            m.put.cnv.enabled = false;
            m.put.image.enabled = false;
            //m.put.graph.enabled = false;
            m.put.array.enabled = false;
            m.put.k12_menu.enabled = false;

            m.media.audio_on.enabled = true;
            m.media.audio_off.enabled = true;
            m.media.test.enabled = true;
            m.media.loadmeter.enabled = true;

            m.win.nextwin.enabled = true;
            m.win.prevwin.enabled = true;
            m.win.parentwin.enabled = true;
            m.win.visible_ancestor.enabled = true;
            m.win.pdwin.enabled = true;
            m.win.abstractions.enabled = true;

            m.help.about.enabled = true;
            m.help.manual.enabled = true;
            m.help.browser.enabled = true;
            m.help.intro.enabled = true;
            m.help.l2ork_list.enabled = true;
            m.help.pd_list.enabled = true;
            m.help.forums.enabled = true;
            m.help.irc.enabled = true;
            m.help.devtools.enabled = true;
        } else {
            //pdgui.post("osx normal");
            m.file.save.enabled = true;
            m.file.saveas.enabled = true;
            m.file.message.enabled = true;

            m.edit.paste_clipboard.enabled = true;
            m.edit.reselect.enabled = true;
            m.edit.clear_console.enabled = true;
            m.edit.encapsulate.enabled = true;
            m.edit.font.enabled = true;
            m.edit.find.enabled = true;
            m.edit.findagain.enabled = true;
            m.edit.finderror.enabled = true;

            m.put.object.enabled = true;
            m.put.message.enabled = true;
            m.put.number.enabled = true;
            m.put.symbol.enabled = true;
            m.put.list.enabled = true;
            m.put.comment.enabled = true;
            m.put.dropdown.enabled = true;
            m.put.bang.enabled = true;
            m.put.toggle.enabled = true;
            m.put.number2.enabled = true;
            m.put.vslider.enabled = true;
            m.put.hslider.enabled = true;
            m.put.knob.enabled = true;
            m.put.vradio.enabled = true;
            m.put.hradio.enabled = true;
            m.put.vu.enabled = true;
            m.put.cnv.enabled = true;
            m.put.image.enabled = true;
            //m.put.graph.enabled = true;
            m.put.array.enabled = true;
            m.put.k12_menu.enabled = true;

            m.media.audio_on.enabled = true;
            m.media.audio_off.enabled = true;
            m.media.test.enabled = true;
            m.media.loadmeter.enabled = true;

            m.win.nextwin.enabled = true;
            m.win.prevwin.enabled = true;
            m.win.parentwin.enabled = true;
            m.win.visible_ancestor.enabled = true;
            m.win.pdwin.enabled = true;
            m.win.abstractions.enabled = true;

            m.help.about.enabled = true;
            m.help.manual.enabled = true;
            m.help.browser.enabled = true;
            m.help.intro.enabled = true;
            m.help.l2ork_list.enabled = true;
            m.help.pd_list.enabled = true;
            m.help.forums.enabled = true;
            m.help.irc.enabled = true;
            m.help.devtools.enabled = true;
        }
    }
}

function set_edit_menu_modals(window, state) {
    //pdgui.post("set_edit_menu_modals state=" + state);
    // if we have toplevel scalars disable editing
    if (pdgui.get_toplevel_scalars(window) === 1) {
        //pdgui.post("...has scalars");
        state = false;
    }
    // ico@vt.edu 2022-11-30: the following disables
    // proper handling of the edit menu updates when
    // editable is toggled. Leaving here to see if there
    // are any regressions...
    
    // OSX needs to keep these enabled, otherwise the events won't trigger
    if (process.platform === "darwin") {
        state = true;
    }
    
    m.edit.undo.enabled = state;
    m.edit.redo.enabled = state;
    m.edit.cut.enabled = state;
    m.edit.copy.enabled = state;
    m.edit.paste.enabled = state;
    m.edit.selectall.enabled = state;
    if (m.edit.font)
        m.edit.font.enabled = state;

    update_osx_k12_menu(window);
}

function set_menu_modals(window, state) {
    //pdgui.post("set_menu_modals window=" + window + " state=" + state);
    // ico@vt.edu 2022-06-18: added context_state for patches that should
    // have select options disabled because the patch in question should
    // not be editable.
    //pdgui.post("set_menu_modals window=" + window +
    //    " pdgui.editable=" + pdgui.canvas_menu_get_editable(window));
    var context_state = state;
    // this does not work
    //if (process.platform === "darwin" && !pdgui.canvas_menu_get_editable(window)) {
    //    context_state = false;
    //}
    // ico@vt.edu 2022-11-30: the following disables
    // proper handling of the edit menu updates when
    // editable is toggled. Leaving here to see if there
    // are any regressions...
    /*
    // OSX needs to keep these enabled, otherwise the events won't trigger
    if (process.platform === "darwin") {
        context_state = true;
    }
    */
    // ico@vt.edu 2022-09-28: only patches with arrays should not be editable
    if (pdgui.get_toplevel_scalars(window) === 1) {
        //pdgui.post("...has scalars");
        context_state = false;
    }
    //pdgui.post("final state=" + context_state);
    m.file.save.enabled = context_state;
    m.file.saveas.enabled = context_state;    
    m.file.close.enabled = true;

    m.edit.undo.enabled = context_state;
    m.edit.redo.enabled = context_state;
    m.edit.cut.enabled = context_state;
    m.edit.copy.enabled = context_state;
    m.edit.paste.enabled = context_state;

    m.edit.selectall.enabled = context_state;
    if (m.edit.font)
        m.edit.font.enabled = context_state;
    if (m.edit.encapsulate)
        m.edit.encapsulate.enabled = context_state;
    if (m.edit.tidyup)
        m.edit.tidyup.enabled = context_state;

    m.edit.editmode.enabled = context_state;
    if (m.edit.cordinspector)
        m.edit.cordinspector.enabled = context_state;
    if (m.edit.find)
        m.edit.find.enabled = context_state;
    if (m.edit.findagain)
        m.edit.findagain.enabled = context_state;   
    if (m.edit.finderror)
        m.edit.finderror.enabled = context_state; 

    if (m.edit.paste_clipboard)
        m.edit.paste_clipboard.enabled = context_state;
    if (m.edit.duplicate)
        m.edit.duplicate.enabled = context_state;
    if (m.edit.reselect)
        m.edit.reselect.enabled = context_state;

    if (m.put) {
        m.put.object.enabled = context_state;
        m.put.message.enabled = context_state;
        m.put.number.enabled = context_state;
        m.put.symbol.enabled = context_state;
        m.put.list.enabled = context_state;
        m.put.comment.enabled = context_state;
        m.put.dropdown.enabled = context_state;
        m.put.bang.enabled = context_state;
        m.put.toggle.enabled = context_state;
        m.put.number2.enabled = context_state;
        m.put.vslider.enabled = context_state;
        m.put.hslider.enabled = context_state;
        m.put.knob.enabled = context_state;
        m.put.vradio.enabled = context_state;
        m.put.hradio.enabled = context_state;
        m.put.vu.enabled = context_state;
        m.put.cnv.enabled = context_state;
        m.put.image.enabled = context_state;
        //m.put.graph.enabled = context_state;
        m.put.array.enabled = context_state;
        m.put.k12_menu.enabled = context_state;
    }

    if (m.win) {
        m.win.parentwin.enabled =
            (pdgui.get_k12_mode() === 1 ? false : true);
        m.win.visible_ancestor.enabled =
            (pdgui.get_k12_mode() === 1 ? false : true);
        m.win.pdwin.enabled = true;
    }

    update_osx_k12_menu(window);
}

function set_editmode_checkbox(state) {
    //console.log("pd_canvas.js set_editmode_checkbox state=" + state);
    m.edit.editmode.checked = state;
}

function set_cord_inspector_checkbox(state) {
    m.edit.cordinspector.checked = state;
}

// stop-gap
function menu_generic () {
    alert("Please implement this");
}

function minit(menu_item, options) {
    var key;
    for (key in options) {
        if (options.hasOwnProperty(key)) {
            // For click callbacks, we want to check if canvas state is
            // "none", in which case we don't call them. This is just a
            // hack, though-- it'd be a better UX to disable all menu items
            // when we're in the "none" state.
            menu_item[key] = (key !== "click") ?
                options[key] :
                function() {
                    if (canvas_events.get_state() !== "none") {
                        options[key]();
                    }
            };
        }
    }
}

// returns edit mode for this particular window based on
// whether patchsvg belongs to editmode class. used for
// the menu when it is being recreated.
function get_editmode() {
    var result = document.getElementById("patchsvg").
        classList.contains("editmode");
    //console.log("pd_canvas.js get_editmode result=" + result);
    return result;
}

// used, so that we can reference menu later
var m  = null;

function nw_create_patch_window_menus(gui, name) {
    // if we're on GNU/Linux or Windows, create the menus:
    m = pd_menus.create_menu(gui, "canvas");
    //pdgui.post("nw_create_patch_window_menus");

    // File sub-entries
    // We explicitly enable these menu items because on OSX
    // the console menu disables them. (Same for Edit and Put menu)
    minit(m.file.new_file, { click: pdgui.menu_new });
    minit(m.file.open, {
        click: function(w) {
            var input, chooser,
                span = document.querySelector("#fileDialogSpan");
            // Complicated workaround-- see comment in build_file_dialog_string
            input = pdgui.build_file_dialog_string({
                style: "display: none;",
                type: "file",
                id: "fileDialog",
                nwworkingdir: pdgui.get_pd_opendir(),
                multiple: null,
                // These are copied from pd_filetypes in pdgui.js
                accept: ".pd,.pat,.mxt,.mxb,.help"
            });
            span.innerHTML = input;
            chooser = document.querySelector("#fileDialog");
            // Hack-- we have to set the event listener here because we
            // changed out the innerHTML above
            chooser.onchange = function() {
                var file_array = this.value;
                // reset value so that we can open the same file twice
                this.value = null;
                pdgui.menu_open(file_array);
                console.log("tried to open something");
            };
            chooser.click();
        }
    });
    if (pdgui.get_k12_mode() == 1) {
        minit(m.file.k12_demos, {
            click: function() {
                pdgui.menu_k12_open_demos(name);
            }
        });
    }
    minit(m.file.save, {
        enabled: true,
        click: function () {
            pdgui.canvas_check_geometry(name); // should this go in menu_save?
            pdgui.menu_save(name);
        }
    });
    minit(m.file.saveas, {
        enabled: true,
        click: function (){
            pdgui.canvas_check_geometry(name);
            pdgui.menu_saveas(name);
        }
    });
    // hlkwok@vt.edu 2022-11-15: disable print and message when in k12 mode
    if (pdgui.get_k12_mode() == 0) {
        minit(m.file.print, {
            enabled: true,
            click: function (){
                pdgui.canvas_check_geometry(name);
                pdgui.menu_print(name);
            }
        });
        minit(m.file.message, {
            click: function() { pdgui.menu_send(name); }
        });
    }
    // hlkwok@vt.edu: functionality for k12 mode switch
    minit(m.file.k12_mode, { 
            click: function() { switch_k12_mode(); } 
    });

    minit(m.file.close, {
        enabled: true,
        click: function() {
            pdgui.canvas_check_geometry(name);
            pdgui.menu_close(name);
        }
    });
    if (process.platform !== "darwin") {
        minit(m.file.quit, {
            click: pdgui.menu_quit
        });
    }

    // Edit menu
    minit(m.edit.undo, {
        enabled: true,
        click: function () {
            if (canvas_events.get_state() === "text") {
                document.execCommand("undo", false, null);
            } else {
                pdgui.pdsend(name, "undo");
            }
        }
    });
    minit(m.edit.redo, {
        enabled: true,
        click: function () {
            if (canvas_events.get_state() === "text") {
                document.execCommand("redo", false, null);
            } else {
                pdgui.pdsend(name, "redo");
            }
        }
    });
    minit(m.edit.cut, {
        enabled: true,
        click: function () {
            pdgui.pdsend(name, "cut");
            // ico@vt.edu 2020-10-30 if we are cutting inside find box
            if (canvas_events.get_state() === "search") {
                if (document.getSelection()) {
                    document.execCommand("cut");
                }
            } else if (pdgui.gui_is_gobj_grabbed(name)) {
                // ico@vt.edu 2021-10-08: cutting contents of a grabbed object
                //pdgui.post("gobj_grabbed copy");
                var clipboard = nw.Clipboard.get();
                var activated_gobj =
                    document.getElementsByClassName("activated");
                // currently we only support cutting from gatoms and number2
                // gatoms will belong to the class atom, while number2 will have
                // parent object that will belong to the class iemgui
                if (activated_gobj[0].classList.contains("atom"))
                    clipboard.set(activated_gobj[0].
                        getElementsByClassName("cut")[0].textContent, 'text');
                else if (activated_gobj[0].parentNode.classList.contains("iemgui")) {
                    var output = activated_gobj[0].textContent.replace('>', '');
                    clipboard.set(output, 'text');
                }
                for (var i = 0; i < clipboard.get('text').length; i++) {
                    // Backspace
                    pdgui.canvas_sendkey(name, 1, 0, 8, 0);
                    pdgui.canvas_sendkey(name, 0, 0, 8, 0);
                }
            } else {
                pdgui.pdsend(name, "cut");
            }
        }
    });
    minit(m.edit.copy, {
        enabled: true,
        click: function () {
            // ico@vt.edu 2020-10-30 if we are copying inside find box
            if (canvas_events.get_state() === "search") {
                //pdgui.post("m.edit.copy text");
                if (document.getSelection()) {
                    document.execCommand("copy");
                }
            } else if (pdgui.gui_is_gobj_grabbed(name)) {
                // ico@vt.edu 2021-10-08: copying contents of a grabbed object
                //pdgui.post("gobj_grabbed copy");
                var clipboard = nw.Clipboard.get();
                var activated_gobj =
                    document.getElementsByClassName("activated");
                // currently we only support copying from gatoms and number2
                // gatoms will belong to the class atom, while number2 will have
                // parent object that will belong to the class iemgui
                if (activated_gobj[0].classList.contains("atom"))
                    clipboard.set(activated_gobj[0].
                        getElementsByClassName("copy")[0].textContent, 'text');
                else if (activated_gobj[0].parentNode.classList.contains("iemgui")) {
                    var output = activated_gobj[0].textContent.replace('>', '');
                    clipboard.set(output, 'text');
                }
            } else {
                pdgui.pdsend(name, "copy");
            }
        }
    });
    minit(m.edit.paste, {
        enabled: true,
        click: function () {
            //pdgui.post("m.edit.paste grabbed=" + pdgui.gui_is_gobj_grabbed(name)
            //+ " state=" + canvas_events.get_state());
            if (canvas_events.get_state() === "search") {
                //pdgui.post("pasting inside a box...");
                // ico@vt.edu 2020-10-30: pasting inside find box
                document.execCommand("paste");
            } else if (pdgui.gui_is_gobj_grabbed(name)) {
                // ico@vt.edu 2021-08-20: pasting inside a grabbed object
                //pdgui.post("gobj_grabbed paste <" + nw.Clipboard.get().get('text') + ">");
                var paste_text = nw.Clipboard.get().get('text');
                for (var i = 0; i < paste_text.length; i++) {
                    pdgui.canvas_sendkey(name, 1, 0, paste_text.charCodeAt(i), 0);
                    pdgui.canvas_sendkey(name, 0, 0, paste_text.charCodeAt(i), 0);
                }
            } else {
                pdgui.pdsend(name, "paste");
            }
        }
    });
    // hlkwok@vt.edu 2022-11-24: remove paste_clipboard and reselect from
    // k12 menu
    if (pdgui.get_k12_mode() == 0) {
        minit(m.edit.paste_clipboard, {
            enabled: true,
            click: function () {
	        var clipboard = nw.Clipboard.get();
	        var text = clipboard.get('text');
	        //pdgui.post("** paste from clipboard: "+text);
	        canvas_events.paste_from_pd_file(name, text);
	    }
        });
    }
    minit(m.edit.duplicate, {
        enabled: true,
        click: function () { pdgui.pdsend(name, "duplicate"); }
    });
    minit(m.edit.selectall, {
        enabled: true,
        click: function (evt) {
            if (canvas_events.get_state() === "normal") {
                pdgui.pdsend(name, "selectall");
            } else if (process.platform === "darwin") {
                // big kluge for OSX to select all inside a
                // contenteditable element (needed because
                // the stupid MacBuiltin is buggy-- see pd_menus.js)
                document.execCommand("selectAll", false, null);
            }
            // ico@vt.edu 2020-10-30 if we are pasting inside find box
            if (canvas_events.get_state() === "search") {
                document.execCommand("selectAll");
            }
        }
    });
    if (pdgui.get_k12_mode() == 0) {
        minit(m.edit.reselect, {
            enabled: true,
            click: function() {
                // This is a bit complicated... menu item shortcuts receive
                // key events before the DOM, so we have to make sure to
                // filter out <ctrl-or-cmd-Enter> in the DOM eventhandler
                // in the "normal" keypress callback.
                // We also have to make sure to send the text ahead to Pd
                // to make sure it has it in the before before attempting
                // to "reselect".
                // As we move the editing functionality from the c code to
                // the GUI, this will get less complex.
                if (canvas_events.get_state() === "floating_text" ||
                    canvas_events.get_state() === "text") {
                    canvas_events.text(); // anchor the object
                    canvas_events.create_obj();
                }
                pdgui.pdsend(name, "reselect");
            }
        });
    }
    // hlkwok@vt.edu 2022-11-15: disable certain buttons in edit menu in k12 mode
    if (pdgui.get_k12_mode() == 0) {
        minit(m.edit.clear_console, {
            enabled: true,
            click: pdgui.clear_console
        });
        minit(m.edit.encapsulate, {
            enabled: true,
            click: function() { pdgui.pdsend(name, "encapsulate"); }
        });
    }
    minit(m.edit.tidyup, {
        enabled: true,
        click: function() { pdgui.pdsend(name, "tidy"); }
    });
    if (pdgui.get_k12_mode() == 0) {
        minit(m.edit.font, {
            enabled: true,
            /*click: function () { pdgui.pdsend(name, "menufont"); } */
        });
        minit(m.font.s8, {
            enabled: true,
            click: function () {
                m.font.s8.checked = true;
                m.font.s10.checked = false;
                m.font.s12.checked = false;
                m.font.s16.checked = false;
                m.font.s24.checked = false;
                m.font.s36.checked = false;
                pdgui.gui_menu_font_change_size(name, 8);
            }
        });
        minit(m.font.s10, {
            enabled: true,
            click: function () {
                m.font.s8.checked = false;
                m.font.s10.checked = true;
                m.font.s12.checked = false;
                m.font.s16.checked = false;
                m.font.s24.checked = false;
                m.font.s36.checked = false;
                pdgui.gui_menu_font_change_size(name, 10);
            }
        });
        minit(m.font.s12, {
            enabled: true,
            click: function () {
                m.font.s8.checked = false;
                m.font.s10.checked = false;
                m.font.s12.checked = true;
                m.font.s16.checked = false;
                m.font.s24.checked = false;
                m.font.s36.checked = false;
                pdgui.gui_menu_font_change_size(name, 12);
            }
        });
        minit(m.font.s16, {
            enabled: true,
            click: function () {
                m.font.s8.checked = false;
                m.font.s10.checked = false;
                m.font.s12.checked = false;
                m.font.s16.checked = true;
                m.font.s24.checked = false;
                m.font.s36.checked = false;
                pdgui.gui_menu_font_change_size(name, 16);
            }
        });
        minit(m.font.s24, {
            enabled: true,
            click: function () {
                m.font.s8.checked = false;
                m.font.s10.checked = false;
                m.font.s12.checked = false;
                m.font.s16.checked = false;
                m.font.s24.checked = false;
                m.font.s36.checked = false;
                pdgui.gui_menu_font_change_size(name, 24);
            }
        });
        minit(m.font.s36, {
            enabled: true,
            click: function () {
                m.font.s8.checked = false;
                m.font.s10.checked = false;
                m.font.s12.checked = false;
                m.font.s16.checked = false;
                m.font.s24.checked = false;
                m.font.s36.checked = true;
                pdgui.gui_menu_font_change_size(name, 36);
            }
        });
    }
    minit(m.edit.cordinspector, {
        enabled: true,
        click: function() { pdgui.pdsend(name, "magicglass 0"); }
    });
    if (pdgui.get_k12_mode() == 0) {
        minit(m.edit.find, {
            click: function () {
                var find_bar = document.getElementById("canvas_find"),
                    find_bar_text = document.getElementById("canvas_find_text"),
                    state = find_bar.style.getPropertyValue("display");
                // if there's a box being edited, try to instantiate it in Pd
                instantiate_live_box();
                if (state === "none") {
                    //pdgui.post("m.edit.find state=none");
                    // ico@vt.edu 2021-10-06: send fake mouse up in case we were doing
                    // a selection box, so that the selection does not get stuck 
                    pdgui.pdsend(name, "mouseup_fake");
                    find_bar.style.setProperty("display", "inline");
                    find_bar_text.focus();
                    find_bar_text.select();
                    canvas_events.search();
                } else {
                    find_bar.style.setProperty("display", "none");
                    //find_bar_text.value = "";
                    // "normal" seems to be the only viable state for the
                    // canvas atm.  But if there are other states added later,
                    // we might need to fetch the previous state here.
                    canvas_events.normal();
                    // this resets the last search term so that the next search
                    // starts from the beginning again
                    canvas_events.find_reset();
                }
            }
        });
        minit(m.edit.findagain, {
            enabled: true,
            click: function() {
                var t = document.getElementById("canvas_find_text").value;
                /*
                pdgui.post("m.edit.findagain value=<" + 
                    t +
                    "> last_search_term=<" + canvas_events.get_last_search_term() +
                    "> backend=<" + pdgui.gui_get_backend_search_term() + ">");
                */
                if (canvas_events.get_last_search_term() === "") {
                    //pdgui.post("...A");
                    if (t !== "") {
                        pdgui.pdsend(name, "find",
                        pdgui.encode_for_dialog(t),
                        canvas_events.get_match_words() ? "1" : "0");
                        canvas_events.set_last_search_term(t);
                        pdgui.gui_get_search_origin_canvas(name);
                    }
                } else {
                    var t = document.getElementById("canvas_find_text").value;
                    //pdgui.post("...B");
                    if (t !== "" && t !== pdgui.gui_get_backend_search_term()) {
                        //pdgui.post("canvas find_again B t is not the same");
                        t = pdgui.gui_get_backend_search_term();
                        //canvas_events.set_last_search_term(t);
                        //pdgui.post("search term=<"+t+">");
                        //document.getElementById("canvas_find_text").value = t;
                        if (t === pdgui.gui_get_backend_search_term()) {
                            pdgui.pdsend(name, "findagain");
                            pdgui.gui_close_find_bar_on_new_window_focus(name);
                        } else {
                            pdgui.pdsend(name, "find",
                            pdgui.encode_for_dialog(t),
                            canvas_events.get_match_words() ? "1" : "0");
                            canvas_events.set_last_search_term(t);
                            pdgui.gui_get_search_origin_canvas(name);
                        }
                    } else {
                        pdgui.pdsend(name, "findagain");
                    }
                }
            }
        });
        minit(m.edit.finderror, {
            enabled: true,
            click: function() {
                pdgui.pdsend("pd finderror");
            }
        });
        minit(m.edit.autotips, {
            enabled: true,
            click: menu_generic
        });
    }
    minit(m.edit.editmode, {
        enabled: true,
        click: function() {
            toggle_edit();
        }
    });
    minit(m.edit.preferences, {
        click: pdgui.open_prefs
    });

    // View menu
    minit(m.view.zoomin, {
        enabled: true,
        click: function () {
            nw_window_zoom(name, +1);
        }
    });
    minit(m.view.zoomout, {
        enabled: true,
        click: function () {
            nw_window_zoom(name, -1);
        }
    });
    minit(m.view.optimalzoom, {
        enabled: true,
        click: function () {
            pdgui.gui_canvas_optimal_zoom(name, 1, 1);
            pdgui.gui_canvas_get_scroll(name);
        }
    });
    minit(m.view.horizzoom, {
        enabled: true,
        click: function () {
            pdgui.gui_canvas_optimal_zoom(name, 1, 0);
            pdgui.gui_canvas_get_scroll(name);
        }
    });
    minit(m.view.vertzoom, {
        enabled: true,
        click: function () {
            pdgui.gui_canvas_optimal_zoom(name, 0, 1);
            pdgui.gui_canvas_get_scroll(name);
        }
    });
    minit(m.view.zoomreset, {
        enabled: true,
        click: function () {
            gui.Window.get().zoomLevel = 0;
            setTimeout(function() { update_k12_menu(); }, 0);
            pdgui.pdsend(name, "zoom", 0);
            pdgui.gui_canvas_get_scroll(name);
        }
    });
    minit(m.view.fullscreen, {
        click: function() {
            var win = gui.Window.get();
            win.toggleFullscreen();
            pdgui.gui_canvas_get_scroll(name);
        }
    });

    if (pdgui.get_k12_mode() == 0)
    {
        // Put menu
        minit(m.put.object, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "obj 0");
            }
        });
        minit(m.put.message, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "msg 0");
            }
        });
        minit(m.put.number, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "floatatom 0");
            }
        });
        minit(m.put.symbol, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "symbolatom 0");
            }
        });
        minit(m.put.list, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "listbox 0");
            }
        });
        minit(m.put.comment, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "text 0");
            }
        });
        minit(m.put.dropdown, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "dropdown 0");
            }
        });
        minit(m.put.bang, {
            enabled: true,
            click: function(e) {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "bng 0");
            }
        });
        minit(m.put.toggle, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "toggle 0");
            }
        });
        minit(m.put.number2, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "numbox 0");
            }
        });
        minit(m.put.vslider, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "vslider 0");
            }
        });
        minit(m.put.hslider, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "hslider 0");
            }
        });
        minit(m.put.knob, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "obj_abstraction flatgui/knob -16 -16");
            }
        });
        minit(m.put.vradio, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "vradio 0");
            }
        });
        minit(m.put.hradio, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "hradio 0");
            }
        });
        minit(m.put.vu, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "vumeter 0");
            }
        });
        minit(m.put.cnv, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "mycnv 0");
            }
        });
        minit(m.put.image, {
            enabled: true,
            click: function() {
                update_live_box();
                pdgui.pdsend(name, "dirty 1");
                pdgui.pdsend(name, "obj_abstraction ggee/image 0 0");
            }
        });
        //minit(m.put.graph, {
        //    enabled: true,
        //    click: function() {
        //        update_live_box();
        //        pdgui.pdsend(name, "dirty 1");
        //        // leaving out some placement logic... see pd.tk menu_graph
        //        pdgui.pdsend(name, "graph NULL 0 0 0 0 30 30 0 30");
        //    },
        //});

        minit(m.put.array, {
            enabled: true,
            click: function() {
                    update_live_box();
                    pdgui.pdsend(name, "dirty 1");
                    pdgui.pdsend(name, "menuarray");
                }
        });

        // hlkwok@vt.edu: 2022-11-15: functionality for toggling k12 menu visibility
        minit(m.put.k12_menu, { 
                click: function() { toggle_k12_menu_visibility(name, m.put.k12_menu.checked); } 
        });
    }

    // Window
    minit(m.win.nextwin, {
        click: function() {
            pdgui.raise_next(name);
        }
    });
    minit(m.win.prevwin, {
        click: function() {
            pdgui.raise_prev(name);
        }
    });
    minit(m.win.parentwin, {
        enabled: true,
        click: function() {
            pdgui.pdsend(name, "findparent", 0);
        }
    });
    minit(m.win.visible_ancestor, {
        enabled: true,
        click: function() {
            pdgui.pdsend(name, "findparent", 1);
        }
    });
    minit(m.win.pdwin, {
        enabled: true,
        click: function() {
            pdgui.raise_pd_window();
        }
    });
    minit(m.win.abstractions, {
        enabled: true,
        click: function () {
            pdgui.pdsend(name, "getabstractions");
        }
    });

    // Media menu
    minit(m.media.audio_on, {
        click: function() {
            pdgui.pdsend("pd dsp 1");
        }
    });
    minit(m.media.audio_off, {
        click: function() {
            pdgui.pdsend("pd dsp 0");
        }
    });
    minit(m.media.test, {
        click: function() {
            pdgui.pd_doc_open("doc/7.stuff/tools", "testtone.pd", 1);
        }
    });
    minit(m.media.loadmeter, {
        click: function() {
            pdgui.pd_doc_open("doc/7.stuff/tools", "load-meter.pd", 1);
        }
    });

    // Help menu
    minit(m.help.about, {
        click: function() {
            pdgui.pd_doc_open("doc/about", "about.pd", 1);
        }
    });
    minit(m.help.manual, {
        click: function() {
            pdgui.pd_doc_open("doc/1.manual", "index.htm");
        }
    });
    minit(m.help.browser, {
        click: pdgui.open_search
    });
    minit(m.help.intro, {
        click: function() {
            pdgui.pd_doc_open("doc/5.reference", "help-intro.pd", 1);
        }
    });
    minit(m.help.l2ork_list, {
        click: function() {
            pdgui.external_doc_open("http://disis.music.vt.edu/listinfo/l2ork-dev");
        }
    });
    minit(m.help.pd_list, {
        click: function() {
            pdgui.external_doc_open("http://puredata.info/community/lists");
        }
    });
    minit(m.help.forums, {
        click: function() {
            pdgui.external_doc_open("http://forum.pdpatchrepo.info/");
        }
    });
    minit(m.help.irc, {
        click: function() {
            pdgui.external_doc_open("http://puredata.info/community/IRC");
        }
    });
    minit(m.help.devtools, {
        click: function () {
            gui.Window.get().showDevTools();
        }
    });

    // update checkboxes
    if (pdgui.get_k12_mode()) {
        m.file.k12_mode.checked = 1;
        document.getElementById("patchsvg").classList.add("k12_mode");
    } else {
        m.file.k12_mode.checked = 0;
        document.getElementById("patchsvg").classList.remove("k12_mode");
        if (pdgui.get_k12_menu_vis()) {
            m.put.k12_menu.checked = 1;
        } else {
            m.put.k12_menu.checked = 0;
        }
    }
    //console.log("nw_create_patch_window_menus get_editmode=" + get_editmode());
    m.edit.editmode.checked = get_editmode();

    // ico@vt.edu 2022-06-18: here we disable edit and inspector options
    // if the patch is not editable (e.g. has only an array inside it).
    // this prevents users from clicking on the edit and cord inspector,
    // or using keyboard shortcuts to create check boxes in the edit menu
    // when the patch should disallow this. this is a good place to do so
    // because it takes place after the menu has been created...
    //pdgui.post("...has scalars? " + pdgui.get_toplevel_scalars(name));
    if (pdgui.get_toplevel_scalars(name)) {
        m.edit.editmode.checked = false;
        m.edit.editmode.enabled = false;
        m.edit.cordinspector.checked = false;
        m.edit.cordinspector.enabled = false;
        m.edit.cut.enabled = false;
        m.edit.copy.enabled = false;
        m.edit.paste.enabled = false;
        m.edit.undo.enabled = false;
        m.edit.redo.enabled = false;
        m.edit.undo.enabled = false;
        m.edit.redo.enabled = false;
        m.edit.selectall.enabled = false;
        if (m.edit.font)
            m.edit.font.enabled = false;
        if (m.edit.encapsulate)
            m.edit.encapsulate.enabled = false;
        if (m.edit.tidyup)
            m.edit.tidyup.enabled = false;
        if (m.edit.paste_clipboard)
            m.edit.paste_clipboard.enabled = false;
        if (m.edit.duplicate)
            m.edit.duplicate.enabled = false;
        if (m.edit.reselect)
            m.edit.reselect.enabled = false;

        if (m.put) {
            m.put.object.enabled = false;
            m.put.message.enabled = false;
            m.put.number.enabled = false;
            m.put.symbol.enabled = false;
            m.put.list.enabled = false;
            m.put.comment.enabled = false;
            m.put.dropdown.enabled = false;
            m.put.bang.enabled = false;
            m.put.toggle.enabled = false;
            m.put.number2.enabled = false;
            m.put.vslider.enabled = false;
            m.put.hslider.enabled = false;
            m.put.knob.enabled = false;
            m.put.vradio.enabled = false;
            m.put.hradio.enabled = false;
            m.put.vu.enabled = false;
            m.put.cnv.enabled = false;
            m.put.image.enabled = false;
            //m.put.graph.enabled = false;
            m.put.array.enabled = false;

            m.put.k12_menu.enabled = false;
        }
    }
    // ico@vt.edu 2022-12-13: toggle menu options on OSX
    // since (AFAICT) we cannot add/remove menu contents
    if (pdgui.nw_os_is_osx) {
        update_osx_k12_menu(name);
    } 
}

function init_menu_font_size(size) {
    //pdgui.post("init_menu_font_size " + size);
    m.font.s8.checked = false;
    m.font.s10.checked = false;
    m.font.s12.checked = false;
    m.font.s16.checked = false;
    m.font.s24.checked = false;
    m.font.s36.checked = false;

    switch(size)
    {
        case 8:
            m.font.s8.checked = true;
            break;
        case 10:
            m.font.s10.checked = true;
            break;
        case 12:
            m.font.s12.checked = true;
            break;
        case 16:
            m.font.s16.checked = true;
            break;
        case 24:
            m.font.s24.checked = true;
            break;
        case 36:
            m.font.s36.checked = true;
            break;
    } 
}

// ico@vt.edu 2020-08-24: this is called when the window is finally
// loaded and then asks libpd to tell us what is the font state
// LATER: we can use this to also update the undo state appropriately
// ico@vt.edu 2022-10-10: this is super hack-ish, since we have no
// way of telling if the window has actually displayed, particularly
// for more complex patches...
// ico@vt.edu 2022-11-30: we expand this now to also deal with the
// editable option. Shortened delay to 0 which seems to work fine with
// nwjs 0.67.1.
function update_menu_items(cid, isblank) {
    //console.log("pd_canvas.js update_menu_items");
    setTimeout(function() {
        pdgui.pdsend(cid, "updatemenu"); //fonts and editable option from c

        if (pdgui.get_k12_mode() || pdgui.get_k12_menu_vis())
        {
            var k12m = document.getElementById("k12_menu");
            if (k12m.style.display == "none") {
                // this resets the menu position to default folded
                // and updates the icon
                k12m.style.display = "block";
                pdgui.toggle_k12_menu(canvas_events.get_id(), 0);
                if (m.edit.editmode.checked)
                    pdgui.toggle_k12_menu(canvas_events.get_id(), 1);
            }
        }
        // no need to check if the k12_mode or k12_menu
        // are disabled, because we are just initializing
        // new window, so those would not have been drawn
        // at this point, anyhow.
        update_k12_menu();

        if (pdgui.get_k12_mode() == 1 && isblank)
        {
            pdgui.canvas_check_geometry(cid);
            setTimeout(function() {
                pdgui.menu_saveas(cid);
            }, 750);
        }
    }, 0);
}

// hlkwok@vt.edu 2022-10-12: switches between k12_mode and normal mode
function switch_k12_mode() {
    //console.log("pd_canvas.js switch_k12_mode");
    pdgui.set_k12_mode(Number(m.file.k12_mode.checked));
}

// hlkwok@vt.edu 2022-10-24: functionality for k12 tabs
function toggle_tab(tab_id) {
    //console.log("pd_canvas.js toggle_tab");
    pdgui.toggle_tab(canvas_events.get_id(), tab_id);
}

// hlkwok@vt.edu 2022-11-1: function for placing K12 objects
function place_K12_object(object) {
    //console.log("pd_canvas.js place_k12_object");
    var cid = canvas_events.get_id();
    var object_path = "K12/" + object;
    pdgui.pdsend(cid, "dirty 1");
    pdgui.pdsend(cid, "obj_abstraction " + object_path + " 0 0");
}

function place_k12_comment() {
    pdgui.pdsend(canvas_events.get_id(), "text 0");
}

// hlkwok@vt.edu 2022-11-3: toggles edit mode in both edit menu and k12 menu
// ico@vt.edu 2022-12-08: reworked to simplify edit functions inside pdgui.js
function toggle_edit() {
    //console.log("pd_canvas.js toggle_edit");
    update_live_box();
    pdgui.pdsend(canvas_events.get_id(), "editmode 0");
}

// hlkwok@vt.edu 2022-11-8: updates k12 menu height, which will adjust
// k12 menu scrollbar
function update_k12_menu() {
    //console.log("pd_canvas.js update_k12_menu");
    pdgui.update_k12_menu(canvas_events.get_id());
}

// hlkwok@vt.edu 2022-11-13: toggles k12 menu position (for side arrow button)
function toggle_k12_menu_and_set_editmode(evt) {
    //console.log("pd_canvas.js toggle_k12_menu_and_set_editmode");
    pdgui.pdsend(canvas_events.get_id(), "editmode 0");
}

// hlkwok@vt.edu 2022-11-15: toggles k12 menu visibility (for k12 menu option
// in the put menu
function toggle_k12_menu_visibility() {
    //console.log("pd_canvas.js toggle_k12_menu_visibility checked=" + m.put.k12_menu.checked);
    pdgui.toggle_k12_menu_visibility(canvas_events.get_id(), m.put.k12_menu.checked);
}

// ico@vt.edu 2022-12-09: invoked from pdgui.js for K12_menu purposes
function update_menu() {
    //pdgui.post("pd_canvas.js update_menu");
    nw_create_patch_window_menus(gui, canvas_events.get_id());
    var k12m = document.getElementById("k12_menu");
    if ((pdgui.get_k12_mode() || pdgui.get_k12_menu_vis()) &&
        k12m.style.display == "none") {
        // this resets the menu position to default folded
        // and updates the icon
        k12m.style.display = "block";
        update_k12_menu(canvas_events.get_id());
        if (m.edit.editmode.checked) {
            // visually adjust the menu (stretch the vertical size, etc.)
            pdgui.toggle_k12_menu(canvas_events.get_id(), 1);
            //console.log("...done with pdgui.toggle_k12_menu");
        } else {
            pdgui.toggle_k12_menu(canvas_events.get_id(), 0);
        }
        // no need to call this, since it is already called
        // inside update_k12_menu
        //pdgui.gui_canvas_get_immediate_scroll(canvas_events.get_id());
    } else if ((!pdgui.get_k12_mode() && !pdgui.get_k12_menu_vis()) &&
        k12m.style.display == "block") {
        // here we only ensure that the position is correct, since
        // user may still be in edit menu, so we don't want to use
        // pdgui.toggle_k12_menu, as that will erroneously set the
        // edit icon to runtime mode.
        pdgui.toggle_k12_menu(canvas_events.get_id(), -1);
        // we delay hiding, to let the animation play out first
        setTimeout(function() {
            k12m.style.display = "none";
        }, 700);
    }

    create_popup_menu(canvas_events.get_id());
    //console.log("...done");
}
