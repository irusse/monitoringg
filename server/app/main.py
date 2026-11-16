"""3MESH Access API — Phase 10 fills CRUD. Photos never go to MeshCore."""

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI(title="3MESH Access", version="0.1.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/health")
def health():
    return {"ok": True, "service": "3mesh-access"}


@app.get("/turnstiles")
def list_turnstiles():
    return {"items": []}


@app.get("/employees")
def list_employees():
    return {"items": []}
