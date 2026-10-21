#ifndef INPUT_COMMAND_LIST_H
#define INPUT_COMMAND_LIST_H

// The words players type for commands, in command-number order: entry 0 is command 1, which is
// how cmd_info and the CMD_ numbers are indexed. An empty entry is a command number no typed
// word reaches, and the "\n" entry ends the list.
inline constexpr const char* COMMAND_NAMES[] = {
    "north", /* 1 */
    "east",
    "south",
    "west",
    "up",
    "down",
    "enter",
    "exits",
    "flee",
    "get",
    "drink", /* 11 */
    "eat",
    "wear",
    "wield",
    "look",
    "score",
    "say", /* 17, do not shift */
    "yell", /* shout */
    "tell",
    "inventory",
    "qui", /* 21 */
    "", /* bounce */
    "kill",
    "insult",
    "help",
    "who",
    "emote",
    "echo",
    "stand",
    "sit",
    "rest", /* 31 */
    "sleep",
    "wake",
    "force",
    "transfer",
    "news",
    "equipment",
    "buy",
    "sell",
    "value",
    "list", /* 41 */
    "drop",
    "goto",
    "weather",
    "read",
    "pour",
    "grab",
    "remove",
    "put",
    "shutdow",
    "save", /* 51 */
    "hit",
    "petitio", /* string */
    "give",
    "quit",
    "stat",
    "rank", /* skillset */
    "time",
    "load",
    "purge",
    "shutdown", /* 61 */
    "compare", /* idea */
    "wiztrack", /* typo */
    "sing", /* bug */
    "whisper",
    "cast",
    "at",
    "ask",
    "order",
    "sip",
    "taste", /* 71 */
    "snoop",
    "follow",
    "rent",
    "offer",
    "advance",
    "open",
    "close",
    "lock",
    "unlock",
    "leave", /* 81 */
    "write",
    "hold",
    "hold", /* never used */ /* 84 */
    "sneak",
    "hide",
    "ambush",
    "pick",
    "behead", /* steal */
    "bash",
    "rescue", /* 91 */
    "kick",
    "practice",
    "examine",
    "take",
    "info",
    "'",
    "practise",
    "use",
    "where",
    "levels", /* 101 */
    "wizutil", /* reroll */
    ":",
    "wizset",
    "wizlist",
    "consider",
    "group",
    "restore",
    "return",
    "switch",
    "", /* quaff */ /* 111 */
    "", /* recite */
    "users",
    "", /* immlist */
    "afk", /* noshout */
    "wizhelp",
    "credits",
    "petition", /* compact */
    "pracreset", /* display */
    "poofin",
    "poofout", /* 121 */
    "teleport",
    "gecho",
    "wiznet", /* don't move */
    "search", /* holylight */
    "invis",
    "specialize",
    "set",
    "ungroup",
    "pray", /* notell */
    "wizlock", /* 131 */
    "jig", /* junk */
    "", /* gsay */
    "hunt", /* pardon */
    "disengage", /* murder */
    "alias", /* title */
    ";", /* wiznet */
    "balance",
    "deposit",
    "withdraw",
    "", /* visible */ /* 141 */
    "twohanded", /* quest */
    "concentrate", /* freeze */
    "dc",
    "butcher",
    "assist",
    "split",
    "gtell",
    "reply",
    "onehanded", /* norepeat */
    "knock", /* toggle */ /* 151 */
    "map",
    "check",
    "receive",
    "mail",
    "register", /* holler */
    "vnum",
    "whois", /* fill */
    "uptime",
    "commands",
    "socials", /* 161 */
    "ban",
    "unban",
    "date",
    "zreset",
    "colour",
    "show",
    "next", /* prompt */
    "handbook",
    "policy",
    "color", /* 171 */
    "manual", /* nogossip */
    "findzone", /* noauction */
    "setfree", /* nograts */
    "ride", /* roomflags */
    "noshout",
    "dismount", /* nowiz */
    "pull", /* notitle */
    "whistle", /* thaw */
    "swing", /* unaffect */
    "cls", /* 181 */
    "clear",
    "version",
    "chat", /* gossip */
    "narrate", /* auction */
    "affections", /* grats */
    "", /* donate */
    "page",
    "report",
    "diagnose",
    "reload", /* 191 */
    "syslog",
    "last",
    "slowns",
    "track",
    "whoami",
    "vstat",
    "beep",
    "lose",
    "shape",
    "zone", /* 201 */
    "slay",
    "gather",
    "will",
    "prepare",
    "light",
    "blowout",
    "fame",
    "trash", /* reserved for orc delay */
    "tame",
    "calm", /* 211 */
    "fish",
    "recruit",
    "refollow",
    "stalk",
    "cover",
    "lead",
    "exploits",
    "trophy",
    "trap", /* "trap",   */
    "account", /* 221 */
    "", /* 222: was linkaccount, removed (no way into the game without an account) */
    "whoacct",
    "obj2html",
    "delete",
    "world",
    "top",
    "groll",
    "shoot",
    "recover", // 230
    "retrieve",
    "scan",
    "details",
    "bite",
    "rend", // 235
    "maul",
    "mark",
    "blind",
    "bend",
    "windblast", // 240
    "smash",
    "frenzy",
    "stomp",
    "cleave",
    "overrun", // 245
    "defend",
    "renounce",
    "mob2csv",
    "savebench", // 249
    "", /* 250: reserved for harness, PR #309 */
    "debug",
    "unprotect", // 252
    "vault",
    "gameoptions", // 254
    "restyle", // 255
    "\n"
};

#endif // INPUT_COMMAND_LIST_H
