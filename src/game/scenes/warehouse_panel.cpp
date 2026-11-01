#include "warehouse_panel.h"

#include "../sim/game_state.h"
#include "../ui/ui.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace
{
    constexpr float kPanelWidth  = 340.0f;
    constexpr float kMargin      = 14.0f;
    constexpr float kPanelLeft   = 16.0f;
    constexpr float kTopGap      = 32.0f; // leaves the news line under the HUD readable
    constexpr float kOrderRow    = 50.0f;
    constexpr float kCarrierRow  = 22.0f;
    constexpr float kPaymentRow  = 20.0f;
    constexpr float kOrderButton = 84.0f;
    constexpr float kQueueGap     = 8.0f; // between the main panel and the queue's
    constexpr float kQueueWidth   = 370.0f;
    constexpr float kQueueRow     = 44.0f;
    constexpr float kCancelButton = 64.0f;

    const Color kDim      = { 0.72f, 0.69f, 0.62f, 1.0f };
    const Color kWarning  = { 0.95f, 0.70f, 0.30f, 1.0f };
    const Color kBarTruck = { 0.78f, 0.62f, 0.25f, 1.0f };
    const Color kBarFull  = { 0.80f, 0.45f, 0.20f, 1.0f };
    const Color kBarCrate = { 0.45f, 0.58f, 0.35f, 1.0f };

    int CountLines(const std::string& text)
    {
        return 1 + (int)std::count(text.begin(), text.end(), '\n');
    }

    const char* CarrierActivity(const WarehouseSim& sim, const CrateCarrier& carrier)
    {
        switch (carrier.stage)
        {
        case CrateCarrier::Stage::ToCrate:  return sim.job == TruckJob::Deliver ? "to the truck" : "to the racks";
        case CrateCarrier::Stage::Lifting:  return "lifting a crate";
        case CrateCarrier::Stage::Carrying: return sim.job == TruckJob::Deliver ? "crate to the racks" : "crate to the truck";
        case CrateCarrier::Stage::Putting:  return "putting it down";
        }
        return "";
    }

    void Heading(Ui& ui, float x, float y, const char* text)
    {
        Ui::TextStyle style;
        style.bold = true;
        ui.Label({ x, y }, text, style);
    }

    void OrderRow(Ui& ui, GameState& state, int kindIndex, float x, float y, float width)
    {
        const ShipmentKind& kind = GetShipmentKind(kindIndex);

        Ui::TextStyle name;
        name.bold = true;
        ui.Label({ x, y + 2.0f }, kind.name, name);

        const int  room = GetCrateRoomAfterQueue(state);
        const bool fits = room >= kind.crates;
        char       detail[96];
        if (fits)
            snprintf(detail, sizeof(detail), "%d crates, sells for %s", kind.crates, WarehouseMoneyText(kind.salePerCrate * kind.crates).c_str());
        else
            snprintf(detail, sizeof(detail), "Only %d of %d crates would fit", room, kind.crates);
        Ui::TextStyle detailStyle;
        detailStyle.size  = 14.0f;
        detailStyle.color = fits ? kDim : kWarning;
        ui.Label({ x, y + 23.0f }, detail, detailStyle);

        const char*     why = "";
        Ui::ButtonStyle button;
        button.disabled = !CanOrderShipment(state, kindIndex, &why);
        const UiRect r  = { x + width - kOrderButton, y + 4.0f, kOrderButton, 34.0f };
        if (ui.Button(r, WarehouseMoneyText(kind.cost), button))
            OrderShipment(state, kindIndex);
        if (ui.IsHovered({ x, y, width, kOrderRow - 4.0f }))
        {
            const int ahead = GetTrucksAhead(state);
            char      when[96];
            if (ahead > 0)
                snprintf(when, sizeof(when), "Queued with %d ahead, then the truck takes %d %s", ahead, kind.hours, kind.hours == 1 ? "hour" : "hours");
            else
                snprintf(when, sizeof(when), "The truck arrives in %d %s", kind.hours, kind.hours == 1 ? "hour" : "hours");
            char tip[320];
            snprintf(tip, sizeof(tip), "%d crates of %s: costs %s now.\n%s and brings %d heat.\nSold on, they pay %s (a profit of %s).%s%s",
                     kind.crates, kind.lowerName, WarehouseMoneyText(kind.cost).c_str(), when, (int)kind.heat,
                     WarehouseMoneyText(kind.salePerCrate * kind.crates).c_str(), WarehouseMoneyText(kind.salePerCrate * kind.crates - kind.cost).c_str(),
                     button.disabled ? "\n" : "", button.disabled ? why : "");
            ui.Tooltip(tip);
        }
    }

    // Beside the main panel, so the order buttons don't move as it grows. Only while there's a queue.
    void DrawQueuePanel(Ui& ui, GameState& state, float top)
    {
        const WarehouseSim& sim = state.warehouse;
        if (sim.queue.empty())
            return;

        const float inner = kQueueWidth - 2.0f * kMargin;

        Ui::TextStyle body;
        body.size = 14.0f;

        // A held truck is why nothing here is moving.
        std::string held;
        if (sim.hold == TruckHold::NoCrew)
            held = "Held up: the truck is waiting for someone to carry its crates. Nothing here starts until it leaves.";
        else if (sim.hold == TruckHold::NoRoom)
            held = "Held up: the warehouse is full. Nothing here starts until the truck leaves: Ship out sends it off now.";
        if (!held.empty())
            held = ui.WrapText(held, inner - 4.0f, body.size);
        const float heldHeight = held.empty() ? 0.0f : (float)CountLines(held) * ui.GetLineHeight(body.size) + 8.0f;

        const float left   = kPanelLeft + kPanelWidth + kQueueGap;
        const float height = 44.0f + heldHeight + kQueueRow * (float)sim.queue.size() + 6.0f;
        const float x      = left + kMargin;
        float       y      = top;
        ui.Panel({ left, y, kQueueWidth, height });
        y += 10.0f;

        Ui::TextStyle title;
        title.size = 20.0f;
        title.bold = true;
        ui.Label({ x, y }, "Waiting for the truck", title);
        char count[32];
        snprintf(count, sizeof(count), "%d of %d", (int)sim.queue.size(), kMaxQueuedTrucks);
        Ui::TextStyle countStyle = body;
        countStyle.color = kDim;
        countStyle.align = Ui::Align::Right;
        ui.Label({ x + inner, y + 4.0f }, count, countStyle);
        y += 34.0f;

        if (!held.empty())
        {
            Ui::TextStyle heldStyle = body;
            heldStyle.color = kWarning;
            heldStyle.bold  = true;
            ui.Label({ x, y }, held, heldStyle);
            y += heldHeight;
        }

        int cancel = -1;
        for (int i = 0; i < (int)sim.queue.size(); ++i)
        {
            const QueuedTruck& truck = sim.queue[(size_t)i];

            Ui::TextStyle name;
            name.bold = true;
            ui.Label({ x, y }, std::to_string(i + 1) + ". " + GetQueuedTruckName(truck), name);

            Ui::TextStyle detail = body;
            detail.color = kDim;
            ui.Label({ x, y + 20.0f }, DescribeQueuedTruck(truck), detail);

            const UiRect button = { x + inner - kCancelButton, y, kCancelButton, 20.0f }; // clear of the line below
            Ui::ButtonStyle style;
            style.textSize = 14.0f;
            if (ui.Button(button, "Cancel", style))
                cancel = i;
            if (ui.IsHovered(button))
                ui.Tooltip(truck.paid > 0 ? "Call it off: " + WarehouseMoneyText(truck.paid) + " back." : std::string("Call it off."));
            y += kQueueRow;
        }
        if (cancel >= 0)
            CancelQueuedTruck(state, cancel);
    }
}

void DrawWarehousePanel(Ui& ui, GameState& state)
{
    const WarehouseSim& sim = state.warehouse;

    const float inner = kPanelWidth - 2.0f * kMargin;

    Ui::TextStyle body;
    body.size = 14.0f;
    const std::string status = ui.WrapText(DescribeTruck(state), inner - 4.0f, body.size);
    const float statusHeight = (float)CountLines(status) * ui.GetLineHeight(body.size);

    const float carrierHeight = kCarrierRow * (float)std::max((int)sim.carriers.size(), 1);
    const float paymentHeight = sim.payments.empty() ? 0.0f : 26.0f + kPaymentRow * (float)sim.payments.size();
    const float height = 44.0f                                          // title
                       + statusHeight + 34.0f                           // truck
                       + 34.0f                                          // crates
                       + 26.0f + kOrderRow * (float)GetShipmentKindCount() // orders
                       + 6.0f + kOrderRow                               // ship out
                       + 26.0f + carrierHeight + paymentHeight + 10.0f;

    const float x = kPanelLeft + kMargin;
    float       y = ui.GetContentTop() + kTopGap;
    DrawQueuePanel(ui, state, y);
    ui.Panel({ kPanelLeft, y, kPanelWidth, height });
    y += 10.0f;

    // Title
    Ui::TextStyle title;
    title.size = 20.0f;
    title.bold = true;
    ui.Image({ x, y + 2.0f, 24.0f, 24.0f }, ui.Texture("ui/icons/truck.tex"));
    ui.Label({ x + 32.0f, y }, "Trucking", title);
    y += 34.0f;

    // The truck
    Ui::TextStyle statusStyle = body;
    if (sim.hold != TruckHold::None)
    {
        statusStyle.color = kWarning;
        statusStyle.bold  = true;
    }
    ui.Label({ x, y }, status, statusStyle);
    y += statusHeight + 4.0f;
    ui.ProgressBar({ x, y, inner, 12.0f }, GetTruckProgress(state), sim.hold != TruckHold::None ? kBarFull : kBarTruck);
    y += 30.0f;

    // Crates
    const int  capacity = CrateCapacity(state);
    const bool full     = state.crates >= capacity;
    ui.Image({ x, y + 2.0f, 20.0f, 20.0f }, ui.Texture("ui/icons/crate.tex"));
    char crates[32];
    snprintf(crates, sizeof(crates), "%d / %d crates", state.crates, capacity);
    ui.ProgressBar({ x + 28.0f, y, inner - 28.0f, 22.0f }, capacity > 0 ? (float)state.crates / (float)capacity : 0.0f, full ? kBarFull : kBarCrate, crates);
    y += 34.0f;

    // Orders
    Heading(ui, x, y, "Order a shipment");
    y += 26.0f;
    for (int i = 0; i < GetShipmentKindCount(); ++i)
    {
        OrderRow(ui, state, i, x, y, inner);
        y += kOrderRow;
    }

    // Ship out
    ui.Fill({ x, y, inner, 1.0f }, { 0.55f, 0.45f, 0.25f, 0.6f });
    y += 6.0f;
    {
        const int     count = ShipOutCrateCount(state);
        const int64_t value = ShipOutValue(state);
        Ui::TextStyle name;
        name.bold = true;
        const bool queues = ShipOutWouldQueue(state);
        char label[48];
        if (queues)
            snprintf(label, sizeof(label), "Sell later");
        else
            snprintf(label, sizeof(label), "Sell %d %s", count, count == 1 ? "crate" : "crates");
        ui.Label({ x, y + 2.0f }, label, name);

        Ui::TextStyle detail = body;
        detail.color = kDim;
        std::string detailText = count > 0 ? "Pays " + WarehouseMoneyText(value) + " after it leaves" : std::string("Nothing to sell yet");
        if (queues)
            detailText = "Queues: counted when it loads";
        ui.Label({ x, y + 23.0f }, detailText, detail);

        const char*     why = "";
        Ui::ButtonStyle button;
        button.disabled = !CanShipOut(state, &why);
        if (ui.Button({ x + inner - kOrderButton, y + 4.0f, kOrderButton, 34.0f }, "Ship out", button))
            OrderShipOut(state);
        if (ui.IsHovered({ x, y, inner, kOrderRow - 4.0f }))
        {
            std::string tip = "Sends the truck to take crates, up to its load: your crew carry them out,\nit drives off, and the money comes in later.";
            if (queues)
                tip += "\nThe truck is busy, so this waits its turn: what it takes and pays\ndepends on what's in the warehouse by then.";
            if (button.disabled)
                tip += std::string("\n") + why;
            ui.Tooltip(tip);
        }
        y += kOrderRow;
    }

    // Who is carrying
    Heading(ui, x, y, "Carrying");
    y += 26.0f;
    if (sim.carriers.empty())
    {
        Ui::TextStyle none = body;
        none.color = kDim;
        ui.Label({ x, y }, "Nobody", none);
    }
    float rowY = y;
    for (const CrateCarrier& carrier : sim.carriers)
    {
        if (const CrewMember* member = state.FindCrew(carrier.crewId))
        {
            ui.Fill({ x, rowY + 3.0f, 12.0f, 12.0f }, member->tint);
            ui.Label({ x + 20.0f, rowY }, member->name, body);
            Ui::TextStyle what = body;
            what.color = kDim;
            what.align = Ui::Align::Right;
            ui.Label({ x + inner, rowY }, CarrierActivity(sim, carrier), what);
        }
        rowY += kCarrierRow;
    }
    y += carrierHeight;

    // Money on its way
    if (!sim.payments.empty())
    {
        y += 4.0f;
        Heading(ui, x, y, "Money due");
        y += 22.0f;
        for (const PendingPayment& payment : sim.payments)
        {
            Ui::TextStyle line = body;
            line.color = { 0.65f, 0.90f, 0.50f, 1.0f };
            char text[64];
            snprintf(text, sizeof(text), "%s in %.1f hours", WarehouseMoneyText(payment.amount).c_str(), payment.minutesLeft / 60.0f);
            ui.Label({ x, y }, text, line);
            y += kPaymentRow;
        }
    }
}
