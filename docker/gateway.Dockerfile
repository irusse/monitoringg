FROM python:3.12-slim
# Optional image for gateway host tools (meshcore-cli).
RUN pip install meshcore-cli
WORKDIR /work
