const fs = require('fs');
const path = require('path');

// 读取 TypeScript 文件并提取版本号
const constFilePath = path.join(__dirname, 'src', 'util', 'const.ts');

let version;

try {
  const fileContent = fs.readFileSync(constFilePath, 'utf8');
  
  // 使用正则表达式匹配 WEB_VERSION 的值，支持单引号和双引号
  const versionMatch = fileContent.match(/const WEB_VERSION\s*=\s*['"`]([^'"`]+)['"`]/);
  
  if (versionMatch && versionMatch[1]) {
    version = versionMatch[1];
    console.log(`从 const.ts 中读取到版本号: ${version}`);
  } else {
    throw new Error('未找到有效的 WEB_VERSION 变量');
  }
} catch (error) {
  console.warn(`无法读取或解析版本文件: ${error.message}`);
  console.log('使用默认版本号');

  version = '1.0.0 20251124';
}

const outputPath = path.join(__dirname, 'dist', '', 'version.txt');

// 确保目录存在
const dir = path.dirname(outputPath);
if (!fs.existsSync(dir)) {
  fs.mkdirSync(dir, { recursive: true });
}

fs.writeFileSync(outputPath, version);
console.log(`Generated ${outputPath} with version ${version}`);