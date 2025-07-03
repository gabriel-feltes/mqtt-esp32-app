# Dockerfile
FROM python:3.11-slim

WORKDIR /app

# Instala os certificados de autoridade necessários para conexões HTTPS/MQTTS
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY mqtt_to_influx.py .

CMD ["python", "mqtt_to_influx.py"]
