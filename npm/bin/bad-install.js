#!/usr/bin/env node
"use strict";

const { main } = require("../scripts/postinstall");

main().catch((error) => {
  console.error(`[bad] Install failed: ${error.message}`);
  process.exit(1);
});
