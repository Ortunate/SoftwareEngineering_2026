"""依据导出资源指纹保留人工验收；任何资产或配套渲染变化都会要求重新验收。"""
import hashlib,json
from pathlib import Path

def fingerprint(directory,dependencies=()):
    """输入资产目录和附属渲染文件；忽略验收文档、元数据和不稳定的 blend 文件头。"""
    files=[p for p in Path(directory).iterdir() if p.suffix in ('.gltf','.bin','.png')]+list(dependencies)
    digest=hashlib.sha256()
    for p in sorted(files,key=lambda p:str(p)):
        digest.update(p.name.encode());digest.update(p.read_bytes())
    return digest.hexdigest()

def carry_acceptance(meta,previous_path,dependencies=()):
    """在写回元数据前调用；仅同指纹且原已通过才保留 approved。"""
    path=Path(previous_path);current=fingerprint(path.parent,dependencies)
    previous=json.loads(path.read_text(encoding='utf-8')) if path.exists() else {}
    meta['humanAcceptance']='approved' if previous.get('humanAcceptance')=='approved' and previous.get('acceptanceFingerprint')==current else 'pending'
    meta['acceptanceFingerprint']=current
