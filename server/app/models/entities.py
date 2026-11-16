"""SQLAlchemy models — implement in prompt 10."""

from datetime import datetime
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column
from sqlalchemy import String, DateTime, Boolean, ForeignKey


class Base(DeclarativeBase):
    pass


class Employee(Base):
    __tablename__ = "employees"
    id: Mapped[str] = mapped_column(String(36), primary_key=True)
    full_name: Mapped[str] = mapped_column(String(200))
    employee_number: Mapped[str] = mapped_column(String(64), default="")
    department: Mapped[str] = mapped_column(String(120), default="")
    position: Mapped[str] = mapped_column(String(120), default="")
    photo_path: Mapped[str] = mapped_column(String(512), default="")
    enabled: Mapped[bool] = mapped_column(Boolean, default=True)
    valid_from: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)
    valid_to: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)


class Card(Base):
    __tablename__ = "cards"
    id: Mapped[str] = mapped_column(String(36), primary_key=True)
    employee_id: Mapped[str] = mapped_column(ForeignKey("employees.id"))
    uid: Mapped[str] = mapped_column(String(32), index=True)
    format: Mapped[str] = mapped_column(String(16), default="wiegand26")
    enabled: Mapped[bool] = mapped_column(Boolean, default=True)


class AccessEvent(Base):
    __tablename__ = "access_events"
    uuid: Mapped[str] = mapped_column(String(36), primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(DateTime)
    turnstile_id: Mapped[str] = mapped_column(String(32))
    employee_id: Mapped[str] = mapped_column(String(36), default="")
    card_uid: Mapped[str] = mapped_column(String(32), default="")
    action: Mapped[str] = mapped_column(String(32))
    result: Mapped[str] = mapped_column(String(16))
    source: Mapped[str] = mapped_column(String(16))
