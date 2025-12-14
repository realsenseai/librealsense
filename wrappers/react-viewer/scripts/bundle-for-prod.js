/**
 * Bundle React build for production deployment with FastAPI
 * 
 * This script copies the Vite build output to the FastAPI static directory,
 * enabling single-server deployment.
 * 
 * Usage: npm run bundle
 */

import { promises as fs } from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const SOURCE_DIR = path.join(__dirname, '..', 'dist');
const TARGET_DIR = path.join(__dirname, '..', '..', 'rest-api', 'static');

async function copyDir(src, dest) {
  await fs.mkdir(dest, { recursive: true });
  const entries = await fs.readdir(src, { withFileTypes: true });

  for (const entry of entries) {
    const srcPath = path.join(src, entry.name);
    const destPath = path.join(dest, entry.name);

    if (entry.isDirectory()) {
      await copyDir(srcPath, destPath);
    } else {
      await fs.copyFile(srcPath, destPath);
    }
  }
}

async function main() {
  console.log('📦 Bundling React build for production...\n');

  // Check if dist exists
  try {
    await fs.access(SOURCE_DIR);
  } catch {
    console.error('❌ Error: dist/ directory not found.');
    console.error('   Run "npm run build" first.\n');
    process.exit(1);
  }

  // Clean target directory
  try {
    await fs.rm(TARGET_DIR, { recursive: true, force: true });
  } catch {
    // Ignore if doesn't exist
  }

  // Copy files
  console.log(`📁 Copying from: ${SOURCE_DIR}`);
  console.log(`📁 Copying to:   ${TARGET_DIR}\n`);

  await copyDir(SOURCE_DIR, TARGET_DIR);

  console.log('✅ Build bundled successfully!');
  console.log('\n📝 To serve from FastAPI, add static file mounting:');
  console.log(`
   from fastapi.staticfiles import StaticFiles
   
   app.mount("/", StaticFiles(directory="static", html=True), name="static")
  `);
}

main().catch(console.error);
